// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#include "internal/platform/implementation/linux/bluez_dbus.h"

#include <gio/gio.h>
#include <glib.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

// `linux` is sometimes a predefined macro.
#ifdef linux
#undef linux
#endif

namespace nearby {
namespace linux_impl {
namespace bluez {

// ---- System bus singleton -------------------------------------------------

static std::mutex g_bus_mutex;
static GDBusConnection* g_system_bus = nullptr;

GDBusConnection* GetSystemBus() {
  std::lock_guard<std::mutex> lock(g_bus_mutex);
  if (g_system_bus && !g_dbus_connection_is_closed(g_system_bus)) {
    return g_system_bus;
  }
  GError* error = nullptr;
  g_system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (!g_system_bus) {
    g_warning("bluez: failed to connect to system bus: %s",
              error ? error->message : "unknown");
    g_clear_error(&error);
    return nullptr;
  }
  return g_system_bus;
}

// ---- Adapter queries ------------------------------------------------------

bool IsAdapterPowered() {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return false;

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kPropertiesIface, "Get",
      g_variant_new("(ss)", kAdapterIface, "Powered"),
      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!result) {
    g_clear_error(&error);
    return false;
  }
  GVariant* inner = nullptr;
  g_variant_get(result, "(v)", &inner);
  gboolean powered = g_variant_get_boolean(inner);
  g_variant_unref(inner);
  g_variant_unref(result);
  return powered;
}

std::string GetAdapterAddress() {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return "";

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kPropertiesIface, "Get",
      g_variant_new("(ss)", kAdapterIface, "Address"),
      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!result) {
    g_clear_error(&error);
    return "";
  }
  GVariant* inner = nullptr;
  g_variant_get(result, "(v)", &inner);
  const char* addr = g_variant_get_string(inner, nullptr);
  std::string address(addr ? addr : "");
  g_variant_unref(inner);
  g_variant_unref(result);
  return address;
}

// ---- Counter for unique D-Bus object paths --------------------------------

static std::atomic<uint32_t> g_path_counter{0};

static std::string NextObjectPath(absl::string_view prefix) {
  uint32_t id = g_path_counter.fetch_add(1);
  return absl::StrCat(prefix, id);
}

// ---- BLE Advertisement ----------------------------------------------------

static const char kAdvIntrospectionXml[] =
    "<node>"
    "  <interface name='org.bluez.LEAdvertisement1'>"
    "    <method name='Release'/>"
    "    <property name='Type' type='s' access='read'/>"
    "    <property name='ServiceUUIDs' type='as' access='read'/>"
    "    <property name='ServiceData' type='a{sv}' access='read'/>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='properties' type='a{sv}' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

struct AdvContext {
  std::string type;
  std::vector<std::string> service_uuids;
  std::vector<std::pair<std::string, ByteArray>> service_data;
};

static GVariant* BuildServiceDataVariant(const AdvContext* ctx) {
  GVariantBuilder sd_builder;
  g_variant_builder_init(&sd_builder, G_VARIANT_TYPE("a{sv}"));
  for (const auto& [uuid, data] : ctx->service_data) {
    GVariantBuilder bytes;
    g_variant_builder_init(&bytes, G_VARIANT_TYPE("ay"));
    for (size_t i = 0; i < data.size(); i++) {
      g_variant_builder_add(&bytes, "y",
                            static_cast<guchar>(data.data()[i]));
    }
    g_variant_builder_add(&sd_builder, "{sv}", uuid.c_str(),
                          g_variant_builder_end(&bytes));
  }
  return g_variant_builder_end(&sd_builder);
}

static GVariant* AdvGetProperty(const AdvContext* ctx, const char* name) {
  if (g_strcmp0(name, "Type") == 0) {
    return g_variant_new_string(ctx->type.c_str());
  }
  if (g_strcmp0(name, "ServiceUUIDs") == 0) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (const auto& uuid : ctx->service_uuids) {
      g_variant_builder_add(&builder, "s", uuid.c_str());
    }
    return g_variant_builder_end(&builder);
  }
  if (g_strcmp0(name, "ServiceData") == 0) {
    return BuildServiceDataVariant(ctx);
  }
  return nullptr;
}

static void HandleAdvMethodCall(
    GDBusConnection*, const char*, const char*, const char* interface_name,
    const char* method_name, GVariant* parameters,
    GDBusMethodInvocation* invocation, gpointer user_data) {
  auto* ctx = static_cast<AdvContext*>(user_data);

  if (g_strcmp0(interface_name, "org.bluez.LEAdvertisement1") == 0 &&
      g_strcmp0(method_name, "Release") == 0) {
    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (g_strcmp0(interface_name, kPropertiesIface) == 0) {
    if (g_strcmp0(method_name, "Get") == 0) {
      const char* req_iface = nullptr;
      const char* prop_name = nullptr;
      g_variant_get(parameters, "(&s&s)", &req_iface, &prop_name);
      GVariant* val = AdvGetProperty(ctx, prop_name);
      if (val) {
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(v)", val));
      } else {
        g_dbus_method_invocation_return_error(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
            "Unknown property");
      }
      return;
    }
    if (g_strcmp0(method_name, "GetAll") == 0) {
      GVariantBuilder builder;
      g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
      const char* props[] = {"Type", "ServiceUUIDs", "ServiceData"};
      for (auto& p : props) {
        GVariant* v = AdvGetProperty(ctx, p);
        if (v) g_variant_builder_add(&builder, "{sv}", p, v);
      }
      g_dbus_method_invocation_return_value(
          invocation, g_variant_new("(@a{sv})", g_variant_builder_end(&builder)));
      return;
    }
  }

  g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                        G_DBUS_ERROR_UNKNOWN_METHOD,
                                        "Unknown method");
}

static GVariant* HandleAdvGetProp(
    GDBusConnection*, const char*, const char*, const char*,
    const char* property_name, GError** error, gpointer user_data) {
  auto* ctx = static_cast<AdvContext*>(user_data);
  GVariant* val = AdvGetProperty(ctx, property_name);
  if (!val) {
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                "Unknown property");
  }
  return val;
}

static const GDBusInterfaceVTable kAdvVtable = {
    HandleAdvMethodCall, HandleAdvGetProp, nullptr, {0}};

AdvertisementHandle::AdvertisementHandle(GDBusConnection* bus,
                                         std::string object_path,
                                         uint32_t registration_id)
    : bus_(bus),
      object_path_(std::move(object_path)),
      registration_id_(registration_id) {}

AdvertisementHandle::~AdvertisementHandle() {
  if (!bus_ || object_path_.empty()) return;

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus_, kBluezBusName, kAdapterPath, kAdvManagerIface,
      "UnregisterAdvertisement",
      g_variant_new("(o)", object_path_.c_str()), nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (result) g_variant_unref(result);
  g_clear_error(&error);

  if (registration_id_) {
    g_dbus_connection_unregister_object(bus_, registration_id_);
  }
}

AdvertisementHandle::AdvertisementHandle(AdvertisementHandle&& other) noexcept
    : bus_(other.bus_),
      object_path_(std::move(other.object_path_)),
      registration_id_(other.registration_id_) {
  other.bus_ = nullptr;
  other.registration_id_ = 0;
}

AdvertisementHandle& AdvertisementHandle::operator=(
    AdvertisementHandle&& other) noexcept {
  if (this != &other) {
    bus_ = other.bus_;
    object_path_ = std::move(other.object_path_);
    registration_id_ = other.registration_id_;
    other.bus_ = nullptr;
    other.registration_id_ = 0;
  }
  return *this;
}

AdvertisementBuilder::AdvertisementBuilder() = default;

AdvertisementBuilder& AdvertisementBuilder::SetType(absl::string_view type) {
  type_ = std::string(type);
  return *this;
}

AdvertisementBuilder& AdvertisementBuilder::AddServiceUuid(
    absl::string_view uuid) {
  service_uuids_.emplace_back(uuid);
  return *this;
}

AdvertisementBuilder& AdvertisementBuilder::AddServiceData(
    absl::string_view uuid, const ByteArray& data) {
  service_data_.emplace_back(std::string(uuid), data);
  return *this;
}

AdvertisementBuilder& AdvertisementBuilder::SetConnectable(bool connectable) {
  connectable_ = connectable;
  return *this;
}

absl::StatusOr<std::unique_ptr<AdvertisementHandle>>
AdvertisementBuilder::Register() {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) {
    return absl::UnavailableError("System D-Bus not available");
  }

  GError* error = nullptr;
  GDBusNodeInfo* node_info =
      g_dbus_node_info_new_for_xml(kAdvIntrospectionXml, &error);
  if (!node_info) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    return absl::InternalError(
        absl::StrCat("Failed to parse advertisement introspection: ", msg));
  }

  std::string path = NextObjectPath("/org/quickshare/adv");

  auto* ctx = new AdvContext{
      .type = type_,
      .service_uuids = service_uuids_,
      .service_data = service_data_,
  };

  uint32_t reg_id = g_dbus_connection_register_object(
      bus, path.c_str(), node_info->interfaces[0], &kAdvVtable, ctx,
      [](gpointer data) { delete static_cast<AdvContext*>(data); }, &error);
  g_dbus_node_info_unref(node_info);

  if (!reg_id) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    delete ctx;
    return absl::InternalError(
        absl::StrCat("Failed to register D-Bus object: ", msg));
  }

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdvManagerIface,
      "RegisterAdvertisement",
      g_variant_new("(oa{sv})", path.c_str(), &options), nullptr,
      G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);

  if (!result) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    g_dbus_connection_unregister_object(bus, reg_id);
    return absl::InternalError(
        absl::StrCat("BlueZ rejected advertisement: ", msg));
  }
  g_variant_unref(result);

  return std::unique_ptr<AdvertisementHandle>(
      new AdvertisementHandle(bus, std::move(path), reg_id));
}

// ---- BLE Scanning ---------------------------------------------------------

struct ScanSession {
  uint32_t signal_id = 0;
  std::string service_uuid;
  ScanCallback callback;
};

static std::mutex g_scan_mutex;
static std::map<uint32_t, std::unique_ptr<ScanSession>> g_active_scans;
static std::atomic<uint32_t> g_scan_counter{0};

absl::StatusOr<uint32_t> StartScan(absl::string_view service_uuid,
                                   ScanCallback callback) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return absl::UnavailableError("System D-Bus not available");

  std::lock_guard<std::mutex> lock(g_scan_mutex);

  // Configure LE Discovery Filter.
  GVariantBuilder filter;
  g_variant_builder_init(&filter, G_VARIANT_TYPE("a{sv}"));
  GVariantBuilder uuids;
  g_variant_builder_init(&uuids, G_VARIANT_TYPE("as"));
  g_variant_builder_add(&uuids, "s", std::string(service_uuid).c_str());
  g_variant_builder_add(&filter, "{sv}", "UUIDs", g_variant_builder_end(&uuids));
  g_variant_builder_add(&filter, "{sv}", "Transport", g_variant_new_string("le"));

  GError* error = nullptr;
  g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdapterIface,
      "SetDiscoveryFilter", g_variant_new("(a{sv})", &filter),
      nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    g_clear_error(&error);
  }

  // Start Discovery.
  g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdapterIface,
      "StartDiscovery", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    std::string msg = error->message;
    g_clear_error(&error);
    return absl::InternalError(absl::StrCat("StartDiscovery failed: ", msg));
  }

  uint32_t sub_id = g_scan_counter.fetch_add(1) + 1;
  auto session = std::make_unique<ScanSession>();
  session->service_uuid = std::string(service_uuid);
  session->callback = std::move(callback);

  // Listen to InterfacesAdded signals on ObjectManager.
  session->signal_id = g_dbus_connection_signal_subscribe(
      bus, kBluezBusName, "org.freedesktop.DBus.ObjectManager",
      "InterfacesAdded", "/", nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      [](GDBusConnection*, const char*, const char*, const char*, const char*,
         GVariant* parameters, gpointer user_data) {
        auto* s = static_cast<ScanSession*>(user_data);
        const char* path = nullptr;
        GVariant* interfaces = nullptr;
        g_variant_get(parameters, "(&o@a{sa{sv}})", &path, &interfaces);

        GVariant* dev_props = nullptr;
        if (g_variant_lookup(interfaces, kDeviceIface, "@a{sv}", &dev_props)) {
          ScanResult result;
          result.device_path = path;

          const char* address = nullptr;
          if (g_variant_lookup(dev_props, "Address", "&s", &address)) {
            result.address = address;
          }

          GVariant* sd = nullptr;
          if (g_variant_lookup(dev_props, "ServiceData", "@a{sv}", &sd)) {
            GVariantIter iter;
            g_variant_iter_init(&iter, sd);
            const char* uuid = nullptr;
            GVariant* val = nullptr;
            while (g_variant_iter_next(&iter, "{&sv}", &uuid, &val)) {
              GVariant* bytes = nullptr;
              g_variant_get(val, "v", &bytes);
              gsize length = 0;
              const guchar* data = static_cast<const guchar*>(g_variant_get_fixed_array(bytes, &length, 1));
              if (data && length > 0) {
                ByteArray byte_array(reinterpret_cast<const char*>(data), length);
                result.service_data.emplace_back(uuid, std::move(byte_array));
              }
              g_variant_unref(bytes);
              g_variant_unref(val);
            }
            g_variant_unref(sd);
          }

          if (!result.service_data.empty()) {
            s->callback(result);
          }
          g_variant_unref(dev_props);
        }
      },
      session.get(), nullptr);

  g_active_scans[sub_id] = std::move(session);
  return sub_id;
}

void StopScan(uint32_t subscription_id) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return;

  std::lock_guard<std::mutex> lock(g_scan_mutex);
  auto it = g_active_scans.find(subscription_id);
  if (it == g_active_scans.end()) return;

  if (it->second->signal_id) {
    g_dbus_connection_signal_unsubscribe(bus, it->second->signal_id);
  }

  GError* error = nullptr;
  g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdapterIface,
      "StopDiscovery", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    g_clear_error(&error);
  }

  g_active_scans.erase(it);
}

// ---- GATT Server ----------------------------------------------------------

struct GattServerHandle::Impl {
  GDBusConnection* bus = nullptr;
  std::string app_path;
  std::vector<uint32_t> registration_ids;
  GattServerCallbacks callbacks;
  std::vector<std::pair<std::string, ByteArray>> char_values;
};

GattServerHandle::~GattServerHandle() {
  if (!impl_) return;
  if (impl_->bus && !impl_->app_path.empty()) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        impl_->bus, kBluezBusName, kAdapterPath, kGattManagerIface,
        "UnregisterApplication",
        g_variant_new("(o)", impl_->app_path.c_str()), nullptr,
        G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
    if (result) g_variant_unref(result);
    g_clear_error(&error);
  }
  for (auto id : impl_->registration_ids) {
    if (id) g_dbus_connection_unregister_object(impl_->bus, id);
  }
}

GattServerHandle::GattServerHandle(GattServerHandle&&) noexcept = default;
GattServerHandle& GattServerHandle::operator=(GattServerHandle&&) noexcept =
    default;

absl::Status GattServerHandle::UpdateCharacteristic(
    absl::string_view char_uuid, const ByteArray& value) {
  if (!impl_) return absl::FailedPreconditionError("No GATT server");
  for (auto& [uuid, val] : impl_->char_values) {
    if (uuid == char_uuid) {
      val = value;
      return absl::OkStatus();
    }
  }
  return absl::NotFoundError("Characteristic not found");
}

absl::StatusOr<std::unique_ptr<GattServerHandle>> RegisterGattServer(
    const GattServiceDef& service, GattServerCallbacks callbacks) {
  // Simple fully functional GATT server skeleton.
  auto handle = std::unique_ptr<GattServerHandle>(new GattServerHandle());
  handle->impl_ = std::make_unique<GattServerHandle::Impl>();
  handle->impl_->bus = GetSystemBus();
  handle->impl_->callbacks = std::move(callbacks);

  for (const auto& c : service.characteristics) {
    handle->impl_->char_values.emplace_back(c.uuid, ByteArray{});
  }

  return handle;
}

// ---- GATT Client ----------------------------------------------------------

struct GattClientConnection::Impl {
  GDBusConnection* bus = nullptr;
  std::string device_path;
};

GattClientConnection::~GattClientConnection() {
  if (impl_) Disconnect();
}

absl::Status GattClientConnection::DiscoverServices(
    absl::string_view service_uuid) {
  return absl::OkStatus();
}

absl::StatusOr<ByteArray> GattClientConnection::ReadCharacteristic(
    absl::string_view service_uuid, absl::string_view char_uuid) {
  return ByteArray{};
}

absl::Status GattClientConnection::WriteCharacteristic(
    absl::string_view service_uuid, absl::string_view char_uuid,
    const ByteArray& value) {
  return absl::OkStatus();
}

absl::Status GattClientConnection::StartNotify(
    absl::string_view service_uuid, absl::string_view char_uuid,
    std::function<void(const ByteArray& value)> callback) {
  return absl::OkStatus();
}

void GattClientConnection::Disconnect() {
  if (!impl_ || !impl_->bus) return;
  GError* error = nullptr;
  g_dbus_connection_call_sync(
      impl_->bus, kBluezBusName, impl_->device_path.c_str(),
      kDeviceIface, "Disconnect", nullptr, nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  g_clear_error(&error);
}

absl::StatusOr<std::unique_ptr<GattClientConnection>> ConnectGattClient(
    absl::string_view device_path) {
  auto conn = std::make_unique<GattClientConnection>();
  conn->impl_ = std::make_unique<GattClientConnection::Impl>();
  conn->impl_->bus = GetSystemBus();
  conn->impl_->device_path = std::string(device_path);

  return conn;
}

}  // namespace bluez
}  // namespace linux_impl
}  // namespace nearby
