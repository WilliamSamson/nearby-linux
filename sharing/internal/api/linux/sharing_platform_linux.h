// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SHARING_INTERNAL_API_LINUX_SHARING_PLATFORM_LINUX_H_
#define SHARING_INTERNAL_API_LINUX_SHARING_PLATFORM_LINUX_H_

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <functional>

#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/preference_manager.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/implementation/linux/preferences_manager.h"
#include "location/nearby/sharing/lib/account/account_manager.h"

namespace nearby::sharing::api {

class BluetoothAdapterLinux : public BluetoothAdapter {
 public:
  explicit BluetoothAdapterLinux(nearby::linux_impl::BluetoothAdapter& adapter) : adapter_(adapter) {}
  ~BluetoothAdapterLinux() override = default;

  bool IsPresent() const override { return true; }
  bool IsPowered() const override { return adapter_.IsEnabled(); }
  bool IsLowEnergySupported() const override { return true; }
  bool IsScanOffloadSupported() const override { return true; }
  bool IsAdvertisementOffloadSupported() const override { return true; }
  bool IsExtendedAdvertisingSupported() const override { return true; }
  bool IsPeripheralRoleSupported() const override { return true; }
  PermissionStatus GetOsPermissionStatus() const override { return PermissionStatus::kAllowed; }

  void SetPowered(bool powered, std::function<void()> success_callback,
                  std::function<void()> error_callback) override {
    if (adapter_.SetStatus(powered ? nearby::api::BluetoothAdapter::Status::kEnabled : nearby::api::BluetoothAdapter::Status::kDisabled)) {
      if (success_callback) success_callback();
    } else {
      if (error_callback) error_callback();
    }
  }

  std::optional<std::string> GetAdapterId() const override { return adapter_.GetName(); }
  MacAddress GetAddress() const override { return adapter_.GetMacAddress(); }

  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasObserver(Observer* observer) override { return false; }

 private:
  nearby::linux_impl::BluetoothAdapter& adapter_;
};

class PreferenceManagerLinux : public PreferenceManager {
 public:
  explicit PreferenceManagerLinux(nearby::linux_impl::PreferencesManager& prefs) : prefs_(prefs) {}
  ~PreferenceManagerLinux() override = default;

  void SetBoolean(absl::string_view key, bool value) override { prefs_.SetBoolean(key, value); }
  void SetInteger(absl::string_view key, int value) override { prefs_.SetInteger(key, value); }
  void SetInt64(absl::string_view key, int64_t value) override { prefs_.SetInt64(key, value); }
  void SetString(absl::string_view key, absl::string_view value) override { prefs_.SetString(key, value); }
  void SetTime(absl::string_view key, absl::Time value) override {
    prefs_.SetInt64(key, absl::ToUnixNanos(value));
  }

  void SetBooleanArray(absl::string_view key, absl::Span<const bool> value) override {}
  void SetIntegerArray(absl::string_view key, absl::Span<const int> value) override {}
  void SetInt64Array(absl::string_view key, absl::Span<const int64_t> value) override {}
  void SetStringArray(absl::string_view key, absl::Span<const std::string> value) override {}
  void SetPrivateCertificateArray(absl::string_view key, absl::Span<const PrivateCertificateData> value) override {}
  void SetCertificateExpirationArray(absl::string_view key, absl::Span<const std::pair<std::string, int64_t>> value) override {}

  void SetDictionaryBooleanValue(absl::string_view key, absl::string_view dictionary_item, bool value) override {}
  void SetDictionaryIntegerValue(absl::string_view key, absl::string_view dictionary_item, int value) override {}
  void SetDictionaryInt64Value(absl::string_view key, absl::string_view dictionary_item, int64_t value) override {}
  void SetDictionaryStringValue(absl::string_view key, absl::string_view dictionary_item, std::string value) override {}
  void RemoveDictionaryItem(absl::string_view key, absl::string_view dictionary_item) override {}

  void SetSyncBindingValue(const nearby::sharing::sync::SyncBindingPrefs& value) override {}

  bool GetBoolean(absl::string_view key, bool default_value) const override { return prefs_.GetBoolean(key, default_value); }
  int GetInteger(absl::string_view key, int default_value) const override { return prefs_.GetInteger(key, default_value); }
  int64_t GetInt64(absl::string_view key, int64_t default_value) const override { return prefs_.GetInt64(key, default_value); }
  std::string GetString(absl::string_view key, const std::string& default_value) const override { return prefs_.GetString(key, default_value); }
  absl::Time GetTime(absl::string_view key, absl::Time default_value) const override {
    int64_t nanos = prefs_.GetInt64(key, absl::ToUnixNanos(default_value));
    return absl::FromUnixNanos(nanos);
  }

  std::vector<bool> GetBooleanArray(absl::string_view key, absl::Span<const bool> default_value) const override { return {}; }
  std::vector<int> GetIntegerArray(absl::string_view key, absl::Span<const int> default_value) const override { return {}; }
  std::vector<int64_t> GetInt64Array(absl::string_view key, absl::Span<const int64_t> default_value) const override { return {}; }
  std::vector<std::string> GetStringArray(absl::string_view key, absl::Span<const std::string> default_value) const override { return {}; }
  std::vector<PrivateCertificateData> GetPrivateCertificateArray(absl::string_view key) const override { return {}; }
  std::vector<std::pair<std::string, int64_t>> GetCertificateExpirationArray(absl::string_view key) const override { return {}; }

  std::optional<bool> GetDictionaryBooleanValue(absl::string_view key, absl::string_view dictionary_item) const override { return std::nullopt; }
  std::optional<int> GetDictionaryIntegerValue(absl::string_view key, absl::string_view dictionary_item) const override { return std::nullopt; }
  std::optional<int64_t> GetDictionaryInt64Value(absl::string_view key, absl::string_view dictionary_item) const override { return std::nullopt; }
  std::optional<std::string> GetDictionaryStringValue(absl::string_view key, absl::string_view dictionary_item) const override { return std::nullopt; }

  std::optional<nearby::sharing::sync::SyncBindingPrefs> GetSyncBindingValue() const override { return std::nullopt; }

  void Remove(absl::string_view key) override { prefs_.Remove(key); }
  void RemoveAllBindingConfigs() override {}

  void AddObserver(absl::string_view name, std::function<void(absl::string_view pref_name)> observer) override {}
  void RemoveObserver(absl::string_view name) override {}

 private:
  nearby::linux_impl::PreferencesManager& prefs_;
};

class SharingPlatformLinux : public SharingPlatform {
 public:
  SharingPlatformLinux();
  ~SharingPlatformLinux() override;

  void InitProductIdGetter(absl::string_view (*product_id_getter)()) override {}

  std::unique_ptr<nearby::api::NetworkMonitor> CreateNetworkMonitor(
      std::function<void(bool)> lan_connected_callback,
      std::function<void(bool)> internet_connected_callback) override { return nullptr; }

  BluetoothAdapter& GetBluetoothAdapter() override { return *adapter_wrapper_; }

  nearby::api::FastInitBleBeacon& GetFastInitBleBeacon() override;

  nearby::api::FastInitiationManager& GetFastInitiationManager() override;

  std::unique_ptr<nearby::api::SystemInfo> CreateSystemInfo() override { return nullptr; }

  std::unique_ptr<nearby::api::AppInfo> CreateAppInfo() override { return nullptr; }

  PreferenceManager& GetPreferenceManager() override { return *prefs_wrapper_; }
  AccountManager& GetAccountManager() override { return *account_mgr_; }
  nearby::api::DeviceInfo& GetDeviceInfo() override { return device_info_; }

  std::unique_ptr<PublicCertificateDatabase> CreatePublicCertificateDatabase(
      const FilePath& database_path) override { return nullptr; }

  bool UpdateFileOriginMetadata(std::vector<FilePath>& file_paths) override { return true; }

 private:
  nearby::linux_impl::BluetoothAdapter adapter_;
  nearby::linux_impl::DeviceInfo device_info_;
  std::unique_ptr<nearby::linux_impl::PreferencesManager> prefs_;
  std::unique_ptr<AccountManager> account_mgr_;

  std::unique_ptr<BluetoothAdapterLinux> adapter_wrapper_;
  std::unique_ptr<PreferenceManagerLinux> prefs_wrapper_;
};

}  // namespace nearby::sharing::api

#endif  // SHARING_INTERNAL_API_LINUX_SHARING_PLATFORM_LINUX_H_
