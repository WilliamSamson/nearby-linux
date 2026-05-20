// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLE_MEDIUM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLE_MEDIUM_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez_dbus.h"

namespace nearby {
namespace linux_impl {

// Linux implementation of the BLE medium using BlueZ D-Bus.
class BleMedium : public api::ble::BleMedium {
 public:
  explicit BleMedium(api::BluetoothAdapter& adapter);
  ~BleMedium() override;

  // Advertising
  bool StartAdvertising(
      const api::ble::BleAdvertisementData& advertising_data,
      api::ble::AdvertiseParameters advertise_set_parameters) override;

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble::BleAdvertisementData& advertising_data,
      api::ble::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) override;

  bool StopAdvertising() override;

  // Scanning
  bool StartScanning(const Uuid& service_uuid,
                     api::ble::TxPowerLevel tx_power_level,
                     ScanCallback callback) override;

  bool StopScanning() override;

  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble::TxPowerLevel tx_power_level,
      ScanningCallback callback) override;

  // GATT Server
  std::unique_ptr<api::ble::GattServer> StartGattServer(
      api::ble::ServerGattConnectionCallback callback) override;

  // GATT Client
  std::unique_ptr<api::ble::GattClient> ConnectToGattServer(
      api::ble::BlePeripheral::UniqueId peripheral_id,
      api::ble::TxPowerLevel tx_power_level,
      api::ble::ClientGattConnectionCallback callback) override;

  // BLE Sockets
  std::unique_ptr<api::ble::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override;

  std::unique_ptr<api::ble::BleSocket> Connect(
      const std::string& service_id, api::ble::TxPowerLevel tx_power_level,
      api::ble::BlePeripheral::UniqueId peripheral_id,
      CancellationFlag* cancellation_flag) override;

  bool IsExtendedAdvertisementsAvailable() override;

 private:
  api::BluetoothAdapter& adapter_;
  std::unique_ptr<bluez::AdvertisementHandle> adv_handle_;
  uint32_t scan_subscription_id_ = 0;
  std::mutex peripherals_mutex_;
  std::unordered_map<api::ble::BlePeripheral::UniqueId, std::string>
      peripheral_paths_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLE_MEDIUM_H_
