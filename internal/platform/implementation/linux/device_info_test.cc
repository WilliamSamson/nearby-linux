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

#include "gtest/gtest.h"

namespace nearby {
namespace linux_impl {
namespace {

TEST(DeviceInfoTest, ReportsLaptopType) {
  DeviceInfo info;
  EXPECT_EQ(info.GetDeviceType(), api::DeviceInfo::DeviceType::kLaptop);
}

TEST(DeviceInfoTest, ReportsChromeOsAsClosestType) {
  DeviceInfo info;
  // api::DeviceInfo::OsType has no kLinux; kChromeOs is the closest neighbor
  // and matches the g3 backend's choice. If a real kLinux is added upstream,
  // this assertion should change with it.
  EXPECT_EQ(info.GetOsType(), api::DeviceInfo::OsType::kChromeOs);
}

TEST(DeviceInfoTest, GetOsDeviceNameReturnsSomething) {
  DeviceInfo info;
  auto name = info.GetOsDeviceName();
  // On any reasonably-configured host either /etc/machine-info's
  // PRETTY_HOSTNAME or gethostname() must be non-empty.
  ASSERT_TRUE(name.has_value());
  EXPECT_FALSE(name->empty());
}

TEST(DeviceInfoTest, PathsAreNonEmpty) {
  DeviceInfo info;
  EXPECT_FALSE(info.GetDownloadPath().GetPath().empty());
  EXPECT_FALSE(info.GetTemporaryPath().GetPath().empty());
  EXPECT_FALSE(info.GetLogPath().GetPath().empty());
}

TEST(DeviceInfoTest, ScreenLockListenerRoundtrip) {
  DeviceInfo info;
  // Should not crash; we don't actually fire the callback in this MVP impl.
  info.RegisterScreenLockedListener("foo", [](auto) {});
  info.UnregisterScreenLockedListener("foo");
}

}  // namespace
}  // namespace linux_impl
}  // namespace nearby
