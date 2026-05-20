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
  auto result = bluez::StartScan(uuid_str, [this, shared_cb](
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
    {
      std::lock_guard<std::mutex> lock(peripherals_mutex_);
      peripheral_paths_[uid] = r.device_path;
    }
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

static uint32_t ToBluezFlags(
    api::ble::GattCharacteristic::Property property) {
  uint32_t flags = 0;
  if ((property & api::ble::GattCharacteristic::Property::kRead) ==
      api::ble::GattCharacteristic::Property::kRead) {
    flags |= bluez::GattCharacteristicDef::kRead;
  }
  if ((property & api::ble::GattCharacteristic::Property::kWrite) ==
      api::ble::GattCharacteristic::Property::kWrite) {
    flags |= bluez::GattCharacteristicDef::kWrite;
  }
  if ((property & api::ble::GattCharacteristic::Property::kNotify) ==
      api::ble::GattCharacteristic::Property::kNotify) {
    flags |= bluez::GattCharacteristicDef::kNotify;
  }
  if ((property & api::ble::GattCharacteristic::Property::kIndicate) ==
      api::ble::GattCharacteristic::Property::kIndicate) {
    flags |= bluez::GattCharacteristicDef::kIndicate;
  }
  return flags;
}

class LinuxGattServer : public api::ble::GattServer {
 public:
  explicit LinuxGattServer(api::ble::ServerGattConnectionCallback callback)
      : callback_(std::move(callback)) {}

  absl::optional<api::ble::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      api::ble::GattCharacteristic::Permission permission,
      api::ble::GattCharacteristic::Property property) override {
    if (handle_) {
      LOG(WARNING) << "Cannot add a GATT characteristic after registration";
      return absl::nullopt;
    }
    if (service_def_.uuid.empty()) {
      service_def_.uuid = std::string(service_uuid);
    } else if (service_def_.uuid != std::string(service_uuid)) {
      LOG(WARNING) << "Linux GATT server currently supports one service per instance";
      return absl::nullopt;
    }

    api::ble::GattCharacteristic characteristic{
        .uuid = characteristic_uuid,
        .service_uuid = service_uuid,
        .permission = permission,
        .property = property,
    };
    characteristics_.push_back(characteristic);
    service_def_.characteristics.push_back({
        .uuid = std::string(characteristic_uuid),
        .flags = ToBluezFlags(property),
    });
    return characteristic;
  }

  bool UpdateCharacteristic(const api::ble::GattCharacteristic& characteristic,
                            const ByteArray& value) override {
    absl::Status status = EnsureRegistered();
    if (!status.ok()) {
      LOG(WARNING) << "Unable to register GATT server: " << status;
      return false;
    }
    return handle_->UpdateCharacteristic(std::string(characteristic.uuid), value)
        .ok();
  }

  absl::Status NotifyCharacteristicChanged(
      const api::ble::GattCharacteristic& characteristic, bool confirm,
      const ByteArray& new_value) override {
    absl::Status status = EnsureRegistered();
    if (!status.ok()) return status;
    return handle_->UpdateCharacteristic(std::string(characteristic.uuid),
                                         new_value);
  }

  void Stop() override { handle_.reset(); }

 private:
  absl::Status EnsureRegistered() {
    if (handle_) return absl::OkStatus();
    if (service_def_.uuid.empty() || service_def_.characteristics.empty()) {
      return absl::FailedPreconditionError(
          "GATT server has no registered characteristics");
    }

    bluez::GattServerCallbacks callbacks;
    callbacks.on_read = [this](const std::string& char_uuid, int offset) {
      if (!callback_.on_characteristic_read_cb) return ByteArray{};
      auto characteristic = FindCharacteristic(char_uuid);
      if (!characteristic.has_value()) return ByteArray{};
      std::string value;
      callback_.on_characteristic_read_cb(
          0, *characteristic, offset,
          [&value](absl::StatusOr<absl::string_view> result) {
            if (result.ok()) value = std::string(*result);
          });
      return ByteArray(value);
    };
    callbacks.on_write = [this](const std::string& char_uuid,
                                const ByteArray& value) {
      auto characteristic = FindCharacteristic(char_uuid);
      if (!characteristic.has_value() || !callback_.on_characteristic_write_cb) {
        return;
      }
      callback_.on_characteristic_write_cb(
          0, *characteristic, 0, value.AsStringView(), [](absl::Status) {});
    };
    callbacks.on_subscribe = [this](const std::string& char_uuid) {
      auto characteristic = FindCharacteristic(char_uuid);
      if (characteristic.has_value() && callback_.characteristic_subscription_cb) {
        callback_.characteristic_subscription_cb(*characteristic);
      }
    };
    callbacks.on_unsubscribe = [this](const std::string& char_uuid) {
      auto characteristic = FindCharacteristic(char_uuid);
      if (characteristic.has_value() &&
          callback_.characteristic_unsubscription_cb) {
        callback_.characteristic_unsubscription_cb(*characteristic);
      }
    };

    auto handle = bluez::RegisterGattServer(service_def_, std::move(callbacks));
    if (!handle.ok()) return handle.status();
    handle_ = std::move(*handle);
    return absl::OkStatus();
  }

  absl::optional<api::ble::GattCharacteristic> FindCharacteristic(
      const std::string& char_uuid) const {
    for (const auto& characteristic : characteristics_) {
      if (std::string(characteristic.uuid) == char_uuid) return characteristic;
    }
    return absl::nullopt;
  }

  api::ble::ServerGattConnectionCallback callback_;
  bluez::GattServiceDef service_def_;
  std::vector<api::ble::GattCharacteristic> characteristics_;
  std::unique_ptr<bluez::GattServerHandle> handle_;
};

std::unique_ptr<api::ble::GattServer> BleMedium::StartGattServer(
    api::ble::ServerGattConnectionCallback callback) {
  return std::make_unique<LinuxGattServer>(std::move(callback));
}

// ---- GATT Client ----------------------------------------------------------

class LinuxGattClient : public api::ble::GattClient {
 public:
  LinuxGattClient(std::unique_ptr<bluez::GattClientConnection> conn)
      : conn_(std::move(conn)) {}

  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) override {
    return conn_->DiscoverServices(std::string(service_uuid)).ok();
  }

  absl::optional<api::ble::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid) override {
    return api::ble::GattCharacteristic{
        .uuid = characteristic_uuid,
        .service_uuid = service_uuid,
    };
  }

  absl::optional<std::string> ReadCharacteristic(
      const api::ble::GattCharacteristic& characteristic) override {
    auto result = conn_->ReadCharacteristic(std::string(characteristic.service_uuid),
                                           std::string(characteristic.uuid));
    if (!result.ok()) return absl::nullopt;
    return std::string(result->data(), result->size());
  }

  bool WriteCharacteristic(const api::ble::GattCharacteristic& characteristic,
                           absl::string_view value, WriteType type) override {
    return conn_->WriteCharacteristic(std::string(characteristic.service_uuid),
                                     std::string(characteristic.uuid),
                                     ByteArray(std::string(value))).ok();
  }

  void Disconnect() override { conn_.reset(); }

 private:
  std::unique_ptr<bluez::GattClientConnection> conn_;
};

std::unique_ptr<api::ble::GattClient> BleMedium::ConnectToGattServer(
    api::ble::BlePeripheral::UniqueId peripheral_id,
    api::ble::TxPowerLevel tx_power_level,
    api::ble::ClientGattConnectionCallback callback) {
  std::string device_path;
  {
    std::lock_guard<std::mutex> lock(peripherals_mutex_);
    auto it = peripheral_paths_.find(peripheral_id);
    if (it != peripheral_paths_.end()) {
      device_path = it->second;
    }
  }
  if (device_path.empty()) {
    LOG(WARNING) << "GATT ConnectToGattServer missing device path for peripheral "
                 << peripheral_id;
    return nullptr;
  }

  auto conn = bluez::ConnectGattClient(device_path);
  if (!conn.ok()) {
    LOG(WARNING) << "Failed to connect to GATT server: " << conn.status();
    return nullptr;
  }
  return std::make_unique<LinuxGattClient>(std::move(*conn));
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
