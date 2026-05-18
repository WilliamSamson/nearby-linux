// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_WIFI_LAN_MEDIUM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_WIFI_LAN_MEDIUM_H_

#include <memory>
#include <string>

#include "internal/platform/implementation/wifi_lan.h"

namespace nearby {
namespace linux_impl {

// Linux WiFi LAN socket wrapping a POSIX TCP socket fd.
class WifiLanSocket : public api::WifiLanSocket {
 public:
  explicit WifiLanSocket(int fd);
  ~WifiLanSocket() override;

  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Linux WiFi LAN server socket wrapping a POSIX TCP listen socket.
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  WifiLanServerSocket(int fd, const std::string& ip, int port);
  ~WifiLanServerSocket() override;

  std::string GetIPAddress() const override;
  int GetPort() const override;
  std::unique_ptr<api::WifiLanSocket> Accept() override;
  Exception Close() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Linux WiFi LAN medium using Avahi D-Bus for mDNS and POSIX TCP sockets.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium();
  ~WifiLanMedium() override;

  bool IsNetworkConnected() const override;

  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;

  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string& service_type) override;

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const ServiceAddress& service_address,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanServerSocket> ListenForService(
      int port) override;

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;

  api::UpgradeAddressInfo GetUpgradeAddressCandidates(
      const api::WifiLanServerSocket& server_socket) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_WIFI_LAN_MEDIUM_H_
