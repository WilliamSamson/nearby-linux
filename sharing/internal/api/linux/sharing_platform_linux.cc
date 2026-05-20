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

#include "sharing/internal/api/linux/sharing_platform_linux.h"

#include <memory>
#include <utility>

#include "internal/platform/implementation/linux/paths.h"

namespace nearby::sharing::api {

SharingPlatformLinux::SharingPlatformLinux() {
  std::string pref_dir = nearby::linux_impl::GetXdgConfigHome() + "/quick-share/preferences";
  prefs_ = std::make_unique<nearby::linux_impl::PreferencesManager>(FilePath(pref_dir));
  account_mgr_ = std::make_unique<AccountManager>();

  adapter_wrapper_ = std::make_unique<BluetoothAdapterLinux>(adapter_);
  prefs_wrapper_ = std::make_unique<PreferenceManagerLinux>(*prefs_);
}

SharingPlatformLinux::~SharingPlatformLinux() = default;

namespace {
class FastInitBleBeaconImpl : public nearby::api::FastInitBleBeacon {
 public:
  void SerializeToByteArray() override {}
  void ParseFromByteArray() override {}
};

class FastInitiationManagerImpl : public nearby::api::FastInitiationManager {
 public:
  void StartAdvertising(nearby::api::FastInitBleBeacon::FastInitType type,
                        std::function<void()> callback,
                        std::function<void(nearby::api::FastInitiationManager::Error)> error_callback) override {
    if (error_callback) error_callback(nearby::api::FastInitiationManager::Error::kHardwareNotSupported);
  }
  void StopAdvertising(std::function<void()> callback) override { if (callback) callback(); }
  void StartScanning(std::function<void()> devices_discovered_callback,
                     std::function<void()> devices_not_discovered_callback,
                     std::function<void(nearby::api::FastInitiationManager::Error)> error_callback) override {
    if (error_callback) error_callback(nearby::api::FastInitiationManager::Error::kHardwareNotSupported);
  }
  void StopScanning(std::function<void()> callback) override { if (callback) callback(); }
  bool IsAdvertising() override { return false; }
  bool IsScanning() override { return false; }
};
}

nearby::api::FastInitBleBeacon& SharingPlatformLinux::GetFastInitBleBeacon() {
  static FastInitBleBeaconImpl* beacon = new FastInitBleBeaconImpl();
  return *beacon;
}

nearby::api::FastInitiationManager& SharingPlatformLinux::GetFastInitiationManager() {
  static FastInitiationManagerImpl* manager = new FastInitiationManagerImpl();
  return *manager;
}

}  // namespace nearby::sharing::api
