// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#include "internal/platform/implementation/linux/wifi_lan_medium.h"

#include <arpa/inet.h>
#include <gio/gio.h>
#include <glib.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"
#include "internal/platform/implementation/linux/bluez_dbus.h"
#include "internal/platform/implementation/wifi_utils.h"

#ifdef linux
#undef linux
#endif

namespace nearby {
namespace linux_impl {

// ---- Pipe-based InputStream/OutputStream for TCP sockets ------------------

class TcpInputStream : public InputStream {
 public:
  explicit TcpInputStream(int fd) : fd_(fd) {}
  ~TcpInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    ByteArray buffer(size);
    ssize_t n = ::read(fd_, buffer.data(), size);
    if (n < 0) return {Exception::kIo};
    if (n == 0) return ExceptionOr<ByteArray>(ByteArray{});
    buffer.SetData(buffer.data(), n);
    return ExceptionOr<ByteArray>(std::move(buffer));
  }

  Exception Close() override { return {Exception::kSuccess}; }

 private:
  int fd_;
};

class TcpOutputStream : public OutputStream {
 public:
  explicit TcpOutputStream(int fd) : fd_(fd) {}
  ~TcpOutputStream() override = default;

  Exception Write(absl::string_view data) override {
    const char* ptr = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
      ssize_t n = ::write(fd_, ptr, remaining);
      if (n < 0) return {Exception::kIo};
      ptr += n;
      remaining -= n;
    }
    return {Exception::kSuccess};
  }

  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override { return {Exception::kSuccess}; }

 private:
  int fd_;
};

// ---- WifiLanSocket --------------------------------------------------------

struct WifiLanSocket::Impl {
  int fd;
  TcpInputStream input;
  TcpOutputStream output;
  Impl(int fd) : fd(fd), input(fd), output(fd) {}
};

WifiLanSocket::WifiLanSocket(int fd) : impl_(std::make_unique<Impl>(fd)) {}

WifiLanSocket::~WifiLanSocket() { Close(); }

InputStream& WifiLanSocket::GetInputStream() { return impl_->input; }
OutputStream& WifiLanSocket::GetOutputStream() { return impl_->output; }

Exception WifiLanSocket::Close() {
  if (impl_ && impl_->fd >= 0) {
    ::close(impl_->fd);
    impl_->fd = -1;
  }
  return {Exception::kSuccess};
}

// ---- WifiLanServerSocket --------------------------------------------------

struct WifiLanServerSocket::Impl {
  int fd;
  std::string ip;
  int port;
};

WifiLanServerSocket::WifiLanServerSocket(int fd, const std::string& ip,
                                         int port)
    : impl_(std::make_unique<Impl>(Impl{fd, ip, port})) {}

WifiLanServerSocket::~WifiLanServerSocket() { Close(); }

std::string WifiLanServerSocket::GetIPAddress() const { return impl_->ip; }
int WifiLanServerSocket::GetPort() const { return impl_->port; }

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  struct sockaddr_in client_addr;
  socklen_t len = sizeof(client_addr);
  int client_fd = ::accept(impl_->fd,
                           reinterpret_cast<struct sockaddr*>(&client_addr),
                           &len);
  if (client_fd < 0) return nullptr;
  return std::make_unique<WifiLanSocket>(client_fd);
}

Exception WifiLanServerSocket::Close() {
  if (impl_ && impl_->fd >= 0) {
    ::close(impl_->fd);
    impl_->fd = -1;
  }
  return {Exception::kSuccess};
}

// ---- WifiLanMedium --------------------------------------------------------

struct DiscoverySession {
  std::string browser_path;
  uint32_t signal_id = 0;
  WifiLanMedium::DiscoveredServiceCallback callback;
};

struct WifiLanMedium::Impl {
  std::mutex mutex;
  // Maps service name to active Avahi EntryGroup object path.
  std::map<std::string, std::string> active_advertisements_;
  // Maps service type to active discovery session.
  std::map<std::string, std::unique_ptr<DiscoverySession>> active_discoveries_;
};

WifiLanMedium::WifiLanMedium() : impl_(std::make_unique<Impl>()) {}
WifiLanMedium::~WifiLanMedium() {
  // Clean up all active advertisements and discoveries.
  std::lock_guard<std::mutex> lock(impl_->mutex);
  GDBusConnection* bus = bluez::GetSystemBus();
  if (bus) {
    for (const auto& [name, path] : impl_->active_advertisements_) {
      g_dbus_connection_call_sync(
          bus, "org.freedesktop.Avahi", path.c_str(),
          "org.freedesktop.Avahi.EntryGroup", "Free", nullptr, nullptr,
          G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
    }
    for (const auto& [type, session] : impl_->active_discoveries_) {
      if (session->signal_id) {
        g_dbus_connection_signal_unsubscribe(bus, session->signal_id);
      }
      g_dbus_connection_call_sync(
          bus, "org.freedesktop.Avahi", session->browser_path.c_str(),
          "org.freedesktop.Avahi.ServiceBrowser", "Free", nullptr, nullptr,
          G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
    }
  }
}

bool WifiLanMedium::IsNetworkConnected() const {
  struct ifaddrs* ifaddr = nullptr;
  if (getifaddrs(&ifaddr) != 0) return false;

  bool connected = false;
  for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) continue;
    if (ifa->ifa_addr->sa_family != AF_INET) continue;
    // Skip loopback.
    if (std::string(ifa->ifa_name) == "lo") continue;
    connected = true;
    break;
  }
  freeifaddrs(ifaddr);
  return connected;
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  GDBusConnection* bus = bluez::GetSystemBus();
  if (!bus) return false;

  std::string name = nsd_service_info.GetServiceName();
  std::string type = nsd_service_info.GetServiceType();
  int port = nsd_service_info.GetPort();

  if (impl_->active_advertisements_.count(name)) {
    return false;
  }

  GError* error = nullptr;
  GVariant* group_res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", "/", "org.freedesktop.Avahi.Server",
      "EntryGroupNew", nullptr, G_VARIANT_TYPE("(o)"),
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!group_res) {
    LOG(WARNING) << "Avahi EntryGroupNew failed: " << (error ? error->message : "unknown");
    g_clear_error(&error);
    return false;
  }

  const char* group_path = nullptr;
  g_variant_get(group_res, "(o)", &group_path);

  GVariantBuilder txt_builder;
  g_variant_builder_init(&txt_builder, G_VARIANT_TYPE("aay"));
  for (const auto& [key, val] : nsd_service_info.GetTxtRecords()) {
    std::string record = absl::StrCat(key, "=", val);
    GVariantBuilder bytes;
    g_variant_builder_init(&bytes, G_VARIANT_TYPE("ay"));
    for (char c : record) {
      g_variant_builder_add(&bytes, "y", static_cast<guchar>(c));
    }
    g_variant_builder_add(&txt_builder, "ay", g_variant_builder_end(&bytes));
  }

  GVariant* add_res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", group_path,
      "org.freedesktop.Avahi.EntryGroup", "AddService",
      g_variant_new("(iiussusq@aay)",
                    -1, // AVAHI_IF_UNSPEC
                    -1, // AVAHI_PROTO_UNSPEC
                    0,  // flags
                    name.c_str(),
                    type.c_str(),
                    "", // domain
                    "", // host
                    static_cast<uint16_t>(port),
                    g_variant_builder_end(&txt_builder)),
      nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);

  if (!add_res) {
    LOG(WARNING) << "Avahi AddService failed: " << (error ? error->message : "unknown");
    g_clear_error(&error);
    g_variant_unref(group_res);
    return false;
  }
  g_variant_unref(add_res);

  GVariant* commit_res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", group_path,
      "org.freedesktop.Avahi.EntryGroup", "Commit", nullptr, nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);

  if (!commit_res) {
    LOG(WARNING) << "Avahi Commit failed: " << (error ? error->message : "unknown");
    g_clear_error(&error);
    g_variant_unref(group_res);
    return false;
  }
  g_variant_unref(commit_res);

  impl_->active_advertisements_[name] = group_path;
  g_variant_unref(group_res);
  LOG(INFO) << "Avahi advertised WiFi LAN service: " << name << " on port " << port;
  return true;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  GDBusConnection* bus = bluez::GetSystemBus();
  if (!bus) return false;

  std::string name = nsd_service_info.GetServiceName();
  auto it = impl_->active_advertisements_.find(name);
  if (it == impl_->active_advertisements_.end()) {
    return false;
  }

  std::string path = it->second;
  GError* error = nullptr;
  GVariant* res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", path.c_str(),
      "org.freedesktop.Avahi.EntryGroup", "Free", nullptr, nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!res) {
    g_clear_error(&error);
  } else {
    g_variant_unref(res);
  }

  impl_->active_advertisements_.erase(it);
  LOG(INFO) << "Stopped Avahi advertising for: " << name;
  return true;
}

static void OnAvahiBrowserSignal(
    GDBusConnection* bus, const char* sender_name, const char* object_path,
    const char* interface_name, const char* signal_name, GVariant* parameters,
    gpointer user_data) {
  auto* session = static_cast<DiscoverySession*>(user_data);
  if (g_strcmp0(signal_name, "ItemNew") != 0) return;

  gint32 interface = 0;
  gint32 protocol = 0;
  const char* name = nullptr;
  const char* type = nullptr;
  const char* domain = nullptr;
  guint32 flags = 0;

  g_variant_get(parameters, "(i&s&s&su)", &interface, &protocol, &name, &type,
                &domain, &flags);

  GError* error = nullptr;
  GVariant* res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", "/", "org.freedesktop.Avahi.Server",
      "ResolveService",
      g_variant_new("(iissiu)", interface, protocol, name, type, domain,
                    -1, // AVAHI_PROTO_UNSPEC
                    0), // flags
      G_VARIANT_TYPE("(iississsaay)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr,
      &error);

  if (!res) {
    g_clear_error(&error);
    return;
  }

  gint32 res_interface = 0;
  gint32 res_protocol = 0;
  const char* res_name = nullptr;
  const char* res_type = nullptr;
  const char* res_domain = nullptr;
  const char* res_host = nullptr;
  gint32 res_aprotocol = 0;
  const char* res_address = nullptr;
  guint16 res_port = 0;
  GVariant* res_txt = nullptr;
  guint32 res_flags = 0;

  g_variant_get(res, "(ii&s&s&s&si&sq@aayu)", &res_interface, &res_protocol,
                &res_name, &res_type, &res_domain, &res_host, &res_aprotocol,
                &res_address, &res_port, &res_txt, &res_flags);

  NsdServiceInfo info;
  info.SetServiceName(res_name);
  info.SetServiceType(res_type);
  info.SetPort(res_port);

  // Parse TXT records.
  GVariantIter iter;
  g_variant_iter_init(&iter, res_txt);
  GVariant* child = nullptr;
  while (g_variant_iter_next(&iter, "@ay", &child)) {
    gsize length = 0;
    const guchar* data = static_cast<const guchar*>(g_variant_get_fixed_array(child, &length, 1));
    if (data && length > 0) {
      std::string record(reinterpret_cast<const char*>(data), length);
      size_t eq = record.find('=');
      if (eq != std::string::npos) {
        info.SetTxtRecord(record.substr(0, eq), record.substr(eq + 1));
      }
    }
    g_variant_unref(child);
  }

  // Set IP address in service info.
  info.SetTxtRecord("ip", res_address);

  LOG(INFO) << "Resolved Avahi service: " << res_name << " at " << res_address << ":" << res_port;
  if (session->callback.service_discovered_cb) {
    session->callback.service_discovered_cb(std::move(info));
  }
  g_variant_unref(res);
}

bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  GDBusConnection* bus = bluez::GetSystemBus();
  if (!bus) return false;

  if (impl_->active_discoveries_.count(service_type)) {
    return false;
  }

  GError* error = nullptr;
  GVariant* browser_res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", "/", "org.freedesktop.Avahi.Server",
      "ServiceBrowserNew",
      g_variant_new("(iissu)",
                    -1, // AVAHI_IF_UNSPEC
                    -1, // AVAHI_PROTO_UNSPEC
                    service_type.c_str(),
                    "", // domain
                    0), // flags
      G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);

  if (!browser_res) {
    LOG(WARNING) << "Avahi ServiceBrowserNew failed: " << (error ? error->message : "unknown");
    g_clear_error(&error);
    return false;
  }

  const char* browser_path = nullptr;
  g_variant_get(browser_res, "(o)", &browser_path);

  auto session = std::make_unique<DiscoverySession>();
  session->browser_path = browser_path;
  session->callback = std::move(callback);

  session->signal_id = g_dbus_connection_signal_subscribe(
      bus, "org.freedesktop.Avahi", "org.freedesktop.Avahi.ServiceBrowser",
      "ItemNew", browser_path, nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      OnAvahiBrowserSignal, session.get(), nullptr);

  impl_->active_discoveries_[service_type] = std::move(session);
  g_variant_unref(browser_res);
  LOG(INFO) << "Avahi started service discovery for type: " << service_type;
  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  GDBusConnection* bus = bluez::GetSystemBus();
  if (!bus) return false;

  auto it = impl_->active_discoveries_.find(service_type);
  if (it == impl_->active_discoveries_.end()) {
    return false;
  }

  auto& session = it->second;
  if (session->signal_id) {
    g_dbus_connection_signal_unsubscribe(bus, session->signal_id);
  }

  GError* error = nullptr;
  GVariant* res = g_dbus_connection_call_sync(
      bus, "org.freedesktop.Avahi", session->browser_path.c_str(),
      "org.freedesktop.Avahi.ServiceBrowser", "Free", nullptr, nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!res) {
    g_clear_error(&error);
  } else {
    g_variant_unref(res);
  }

  impl_->active_discoveries_.erase(it);
  LOG(INFO) << "Stopped Avahi discovery for type: " << service_type;
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  std::string ip = remote_service_info.GetTxtRecord("ip");
  int port = remote_service_info.GetPort();
  if (ip.empty()) {
    LOG(WARNING) << "ConnectToService: no IP address in service info";
    return nullptr;
  }

  LOG(INFO) << "Connecting to WiFi LAN service at " << ip << ":" << port;
  ServiceAddress addr{
    .address = std::vector<char>(ip.begin(), ip.end()),
    .port = static_cast<uint16_t>(port)
  };
  return ConnectToService(addr, cancellation_flag);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const ServiceAddress& service_address,
    CancellationFlag* cancellation_flag) {
  std::string ip = WifiUtils::GetHumanReadableIpAddress(
      std::string(service_address.address.begin(), service_address.address.end()));
  int port = service_address.port;

  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return nullptr;

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
    ::close(fd);
    return nullptr;
  }

  if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) != 0) {
    ::close(fd);
    return nullptr;
  }

  return std::make_unique<WifiLanSocket>(fd);
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(
    int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return nullptr;

  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    ::close(fd);
    return nullptr;
  }

  if (::listen(fd, 5) != 0) {
    ::close(fd);
    return nullptr;
  }

  // Get actual port if 0 was requested.
  socklen_t len = sizeof(addr);
  getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
  int actual_port = ntohs(addr.sin_port);

  // Get local IP.
  std::string local_ip = "0.0.0.0";
  struct ifaddrs* ifaddr = nullptr;
  if (getifaddrs(&ifaddr) == 0) {
    for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
      if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
      if (std::string(ifa->ifa_name) == "lo") continue;
      char buf[INET_ADDRSTRLEN];
      auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
      inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
      local_ip = buf;
      break;
    }
    freeifaddrs(ifaddr);
  }

  return std::make_unique<WifiLanServerSocket>(fd, local_ip, actual_port);
}

absl::optional<std::pair<std::int32_t, std::int32_t>>
WifiLanMedium::GetDynamicPortRange() {
  return std::make_pair(49152, 65535);
}

api::UpgradeAddressInfo WifiLanMedium::GetUpgradeAddressCandidates(
    const api::WifiLanServerSocket& server_socket) {
  std::string ip_address = server_socket.GetIPAddress();
  return {
    .num_interfaces = 1,
    .num_ipv6_only_interfaces = 0,
    .address_candidates =
      {ServiceAddress{
      .address = std::vector<char>(ip_address.begin(), ip_address.end()),
      .port = static_cast<uint16_t>(server_socket.GetPort())}}
  };
}

}  // namespace linux_impl
}  // namespace nearby
