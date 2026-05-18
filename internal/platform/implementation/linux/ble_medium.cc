// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#include "internal/platform/implementation/linux/ble_medium.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/linux/bluez_dbus.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"

#ifdef linux
#undef linux
#endif

namespace nearby {
namespace linux_impl {

BleMedium::BleMedium(api::BluetoothAdapter& adapter) : adapter_(adapter) {}

BleMedium::~BleMedium() { StopAdvertising(); }

// ---- Advertising ----------------------------------------------------------

bool BleMedium::StartAdvertising(
    const api::ble::BleAdvertisementData& advertising_data,
    api::ble::AdvertiseParameters params) {
  StopAdvertising();

  bluez::AdvertisementBuilder builder;
  builder.SetType("peripheral");
  builder.SetConnectable(params.is_connectable);

  for (const auto& [uuid, data] : advertising_data.service_data) {
    std::string uuid_str(uuid);
    builder.AddServiceUuid(uuid_str);
    builder.AddServiceData(uuid_str, data);
  }

  auto result = builder.Register();
  if (!result.ok()) {
    LOG(WARNING) << "BLE StartAdvertising failed: " << result.status();
    return false;
  }

  adv_handle_ = std::move(*result);
  LOG(INFO) << "BLE advertising started";
  return true;
}

std::unique_ptr<api::ble::BleMedium::AdvertisingSession>
BleMedium::StartAdvertising(
    const api::ble::BleAdvertisementData& advertising_data,
    api::ble::AdvertiseParameters params, AdvertisingCallback callback) {
  bool ok = StartAdvertising(advertising_data, params);
  if (callback.start_advertising_result) {
    callback.start_advertising_result(
        ok ? absl::OkStatus()
           : absl::InternalError("BLE advertising failed"));
  }
  if (!ok) return nullptr;

  auto session = std::make_unique<AdvertisingSession>();
  session->stop_advertising = [this]() -> absl::Status {
    StopAdvertising();
    return absl::OkStatus();
  };
  return session;
}

bool BleMedium::StopAdvertising() {
  if (adv_handle_) {
    adv_handle_.reset();
    LOG(INFO) << "BLE advertising stopped";
  }
  return true;
}

// ---- Scanning -------------------------------------------------------------

bool BleMedium::StartScanning(const Uuid& service_uuid,
                              api::ble::TxPowerLevel tx_power_level,
                              ScanCallback callback) {
  std::string uuid_str(service_uuid);
  auto shared_cb = std::make_shared<ScanCallback>(std::move(callback));
  auto result = bluez::StartScan(uuid_str, [shared_cb](
                                                const bluez::ScanResult& r) {
    // Convert to BLE advertisement data.
    api::ble::BleAdvertisementData adv_data;
    adv_data.is_extended_advertisement = false;
    for (const auto& [uuid, data] : r.service_data) {
      auto parsed = Uuid::FromString(uuid);
      if (parsed) {
        adv_data.service_data[*parsed] = data;
      }
    }
    // Create a peripheral with address-based unique ID.
    api::ble::BlePeripheral::UniqueId uid =
        std::hash<std::string>{}(r.address);
    if (shared_cb->advertisement_found_cb) {
      shared_cb->advertisement_found_cb(uid, std::move(adv_data));
    }
  });

  if (!result.ok()) {
    LOG(WARNING) << "BLE StartScanning failed: " << result.status();
    return false;
  }
  scan_subscription_id_ = *result;
  return true;
}

bool BleMedium::StopScanning() {
  if (scan_subscription_id_) {
    bluez::StopScan(scan_subscription_id_);
    scan_subscription_id_ = 0;
  }
  return true;
}

std::unique_ptr<api::ble::BleMedium::ScanningSession>
BleMedium::StartScanning(const Uuid& service_uuid,
                         api::ble::TxPowerLevel tx_power_level,
                         ScanningCallback callback) {
  ScanCallback sync_cb;
  sync_cb.advertisement_found_cb =
      [cb = &callback](api::ble::BlePeripheral::UniqueId id,
                       api::ble::BleAdvertisementData data) {
        if (cb->advertisement_found_cb) {
          cb->advertisement_found_cb(id, std::move(data));
        }
      };

  bool ok = StartScanning(service_uuid, tx_power_level, std::move(sync_cb));
  if (callback.start_scanning_result) {
    callback.start_scanning_result(
        ok ? absl::OkStatus()
           : absl::InternalError("BLE scanning failed"));
  }
  if (!ok) return nullptr;

  auto session = std::make_unique<ScanningSession>();
  session->stop_scanning = [this]() -> absl::Status {
    StopScanning();
    return absl::OkStatus();
  };
  return session;
}

// ---- GATT Server ----------------------------------------------------------

std::unique_ptr<api::ble::GattServer> BleMedium::StartGattServer(
    api::ble::ServerGattConnectionCallback callback) {
  // TODO(phase1c): Implement via bluez::RegisterGattServer
  LOG(WARNING) << "GATT server not yet implemented on Linux";
  return nullptr;
}

// ---- GATT Client ----------------------------------------------------------

std::unique_ptr<api::ble::GattClient> BleMedium::ConnectToGattServer(
    api::ble::BlePeripheral::UniqueId peripheral_id,
    api::ble::TxPowerLevel tx_power_level,
    api::ble::ClientGattConnectionCallback callback) {
  // TODO(phase1c): Implement via bluez::ConnectGattClient
  LOG(WARNING) << "GATT client not yet implemented on Linux";
  return nullptr;
}

// ---- BLE Sockets ----------------------------------------------------------

std::unique_ptr<api::ble::BleServerSocket> BleMedium::OpenServerSocket(
    const std::string& service_id) {
  // TODO(phase1d): BLE socket over GATT characteristics
  return nullptr;
}

std::unique_ptr<api::ble::BleSocket> BleMedium::Connect(
    const std::string& service_id, api::ble::TxPowerLevel tx_power_level,
    api::ble::BlePeripheral::UniqueId peripheral_id,
    CancellationFlag* cancellation_flag) {
  // TODO(phase1d): BLE socket over GATT characteristics
  return nullptr;
}

bool BleMedium::IsExtendedAdvertisementsAvailable() {
  // Most Linux BT adapters don't support extended advertising.
  return false;
}

}  // namespace linux_impl
}  // namespace nearby
