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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_DEVICE_INFO_H_
#define PLATFORM_IMPLEMENTATION_LINUX_DEVICE_INFO_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace linux_impl {

class DeviceInfo final : public api::DeviceInfo {
 public:
  DeviceInfo() = default;
  ~DeviceInfo() override = default;

  std::optional<std::string> GetOsDeviceName() const override;
  api::DeviceInfo::DeviceType GetDeviceType() const override;
  api::DeviceInfo::OsType GetOsType() const override;

  FilePath GetDownloadPath() const override;
  FilePath GetLocalAppDataPath(FilePath sub_path) const override;
  FilePath GetTemporaryPath() const override;
  FilePath GetLogPath() const override;

  bool IsScreenLocked() const override;
  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override;
  void UnregisterScreenLockedListener(absl::string_view listener_name) override;

  bool PreventSleep() override;
  bool AllowSleep() override;

 private:
  mutable absl::Mutex listeners_mutex_;
  absl::flat_hash_map<std::string,
                      std::function<void(api::DeviceInfo::ScreenStatus)>>
      screen_locked_listeners_ ABSL_GUARDED_BY(listeners_mutex_);
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_DEVICE_INFO_H_
