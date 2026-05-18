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

#include "internal/platform/implementation/linux/device_info.h"

#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/linux/paths.h"

namespace nearby {
namespace linux_impl {

namespace {

// Returns the contents of /etc/machine-info's PRETTY_HOSTNAME if set,
// otherwise an empty string. systemd writes this when the user sets a friendly
// device name; matches what GNOME/KDE typically display.
std::string ReadPrettyHostname() {
  std::ifstream f("/etc/machine-info");
  if (!f.is_open()) return "";
  std::string line;
  while (std::getline(f, line)) {
    absl::string_view sv = line;
    if (!absl::ConsumePrefix(&sv, "PRETTY_HOSTNAME=")) continue;
    sv = absl::StripPrefix(sv, "\"");
    sv = absl::StripSuffix(sv, "\"");
    return std::string(sv);
  }
  return "";
}

std::string ReadGethostname() {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) != 0) return "";
  buf[sizeof(buf) - 1] = '\0';
  return std::string(buf);
}

}  // namespace

std::optional<std::string> DeviceInfo::GetOsDeviceName() const {
  std::string pretty = ReadPrettyHostname();
  if (!pretty.empty()) return pretty;
  std::string host = ReadGethostname();
  if (!host.empty()) return host;
  return std::nullopt;
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  // Ubuntu desktop is targeted; no laptop-vs-desktop disambiguation needed for
  // Quick Share's purposes.
  return api::DeviceInfo::DeviceType::kLaptop;
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
  // api::DeviceInfo::OsType has no kLinux. kChromeOs is the closest
  // Linux-derived label other peers may recognize; this matches what the
  // existing g3 backend reports.
  return api::DeviceInfo::OsType::kChromeOs;
}

FilePath DeviceInfo::GetDownloadPath() const {
  return FilePath(GetXdgDownloadDir());
}

FilePath DeviceInfo::GetLocalAppDataPath(FilePath sub_path) const {
  return FilePath(GetXdgDataHome()).append(FilePath("quick-share")).append(sub_path);
}

FilePath DeviceInfo::GetTemporaryPath() const {
  const char* tmpdir = std::getenv("TMPDIR");
  return FilePath(tmpdir && *tmpdir ? tmpdir : "/tmp");
}

FilePath DeviceInfo::GetLogPath() const {
  return FilePath(GetXdgDataHome()).append(FilePath("quick-share/logs"));
}

bool DeviceInfo::IsScreenLocked() const {
  // Querying screen lock state requires desktop-environment-specific D-Bus
  // calls (logind, gnome-screensaver, kscreenlocker). Receiver-first MVP does
  // not branch on lock state; returning false is the safe default.
  return false;
}

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  absl::MutexLock lock(listeners_mutex_);
  screen_locked_listeners_.insert_or_assign(std::string(listener_name),
                                            std::move(callback));
}

void DeviceInfo::UnregisterScreenLockedListener(
    absl::string_view listener_name) {
  absl::MutexLock lock(listeners_mutex_);
  screen_locked_listeners_.erase(std::string(listener_name));
}

bool DeviceInfo::PreventSleep() {
  // Real impl would call org.freedesktop.login1.Manager.Inhibit over D-Bus
  // and hold the returned fd for the lifetime of the transfer. Phase B+
  // follow-up; receiver MVP doesn't strictly require it.
  return true;
}

bool DeviceInfo::AllowSleep() { return true; }

}  // namespace linux_impl
}  // namespace nearby
