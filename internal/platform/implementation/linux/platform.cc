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

#include "internal/platform/implementation/platform.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/linux/atomic_boolean.h"
#include "internal/platform/implementation/linux/atomic_reference.h"
#include "internal/platform/implementation/linux/ble_medium.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/credential_storage.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/implementation/linux/multi_thread_executor.h"
#include "internal/platform/implementation/linux/paths.h"
#include "internal/platform/implementation/linux/preferences_manager.h"
#include "internal/platform/implementation/linux/scheduled_executor.h"
#include "internal/platform/implementation/linux/single_thread_executor.h"
#include "internal/platform/implementation/linux/timer.h"
#include "internal/platform/implementation/linux/wifi_lan_medium.h"
#include "internal/platform/implementation/log_message.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/preferences_manager.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/implementation/shared/posix_condition_variable.h"
#include "internal/platform/implementation/shared/posix_mutex.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/implementation/timer.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/os_name.h"
#include "internal/platform/payload_id.h"

// `linux` is sometimes a predefined macro. Undefine it so our namespace and
// any related symbols compile cleanly.
#ifdef linux
#undef linux
#endif

namespace nearby {
namespace api {

// ---- OS identity ----------------------------------------------------------

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kLinux; }

// ---- File paths -----------------------------------------------------------

std::string ImplementationPlatform::GetCustomSavePath(
    const std::string& parent_folder, const std::string& file_name) {
  if (parent_folder.empty()) return file_name;
  return absl::StrCat(parent_folder, "/", file_name);
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string& parent_folder, const std::string& file_name) {
  std::string base = linux_impl::GetXdgDownloadDir();
  if (parent_folder.empty()) {
    return absl::StrCat(base, "/", file_name);
  }
  return absl::StrCat(base, "/", parent_folder, "/", file_name);
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string& file_name) {
  return absl::StrCat(linux_impl::GetXdgDownloadDir(), "/", file_name);
}

std::string ImplementationPlatform::GetAppDataPath(
    const std::string& file_name) {
  return absl::StrCat(linux_impl::GetXdgDataHome(), "/quick-share/", file_name);
}

// ---- Atomics --------------------------------------------------------------

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return std::make_unique<linux_impl::AtomicBoolean>(initial_value);
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return std::make_unique<linux_impl::AtomicUint32>(value);
}

// ---- Synchronization ------------------------------------------------------

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode /*mode*/) {
  // shared::posix::Mutex is non-recursive (no mode parameter). Recursive mode
  // is a Phase B follow-up; current callers in the receiver-first path do not
  // require recursive locking.
  return std::make_unique<posix::Mutex>();
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::make_unique<posix::ConditionVariable>(
      static_cast<posix::Mutex*>(mutex));
}

// ---- Files ----------------------------------------------------------------

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId payload_id) {
  return shared::IOFile::CreateInputFile(GetDownloadPath(std::to_string(payload_id)));
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const std::string& file_path) {
  return shared::IOFile::CreateInputFile(file_path);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    PayloadId payload_id) {
  return shared::IOFile::CreateOutputFile(GetDownloadPath(std::to_string(payload_id)));
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    const std::string& file_path) {
  FilePath path(file_path);
  FilePath folder_path = path.GetParentPath();
  if (!Files::DirectoryExists(folder_path)) {
    if (!Files::CreateDirectories(folder_path)) {
      return nullptr;
    }
  }
  return shared::IOFile::CreateOutputFile(file_path);
}

// ---- Logging --------------------------------------------------------------

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* /*file*/, int /*line*/, LogMessage::Severity /*severity*/) {
  // Matches the g3 backend: log routing is owned by the host process. The
  // Linux app uses GLib's g_log machinery, not this API.
  return nullptr;
}

// ---- Executors / Timer ----------------------------------------------------

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<linux_impl::SingleThreadExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(
    std::int32_t max_concurrency) {
  return std::make_unique<linux_impl::MultiThreadExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return std::make_unique<linux_impl::ScheduledExecutor>();
}

std::unique_ptr<Timer> ImplementationPlatform::CreateTimer() {
  return std::make_unique<linux_impl::Timer>();
}

// ---- Device / Preferences / Credentials -----------------------------------

std::unique_ptr<DeviceInfo> ImplementationPlatform::CreateDeviceInfo() {
  return std::make_unique<linux_impl::DeviceInfo>();
}

std::unique_ptr<PreferencesManager>
ImplementationPlatform::CreatePreferencesManager(absl::string_view path) {
  // Treat empty path as the default XDG location.
  std::string dir = path.empty()
                        ? absl::StrCat(linux_impl::GetXdgConfigHome(),
                                       "/quick-share/preferences")
                        : std::string(path);
  return std::make_unique<linux_impl::PreferencesManager>(FilePath(dir));
}

std::unique_ptr<CredentialStorage>
ImplementationPlatform::CreateCredentialStorage() {
  return std::make_unique<linux_impl::CredentialStorage>();
}

std::unique_ptr<AppLifecycleMonitor>
ImplementationPlatform::CreateAppLifecycleMonitor(
    std::function<void(AppLifecycleMonitor::AppLifecycleState)>
    /*state_updated_callback*/) {
  return nullptr;
}

absl::StatusOr<WebResponse> ImplementationPlatform::SendRequest(
    const WebRequest& /*request*/) {
  return absl::UnimplementedError("HTTP loader not implemented on Linux yet");
}

// ---- Mediums (slices 4-6) -------------------------------------------------

std::unique_ptr<AwdlMedium> ImplementationPlatform::CreateAwdlMedium() {
  return nullptr;
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return std::make_unique<linux_impl::BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& /*adapter*/) {
  return nullptr;
}

std::unique_ptr<api::ble::BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  return std::make_unique<linux_impl::BleMedium>(adapter);
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return nullptr;
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::make_unique<linux_impl::WifiLanMedium>();
}

std::unique_ptr<WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  return nullptr;
}

std::unique_ptr<WifiDirectMedium>
ImplementationPlatform::CreateWifiDirectMedium() {
  return nullptr;
}

}  // namespace api
}  // namespace nearby
