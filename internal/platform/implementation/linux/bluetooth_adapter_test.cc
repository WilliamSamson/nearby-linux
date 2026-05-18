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

#include "internal/platform/implementation/linux/bluetooth_adapter.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "gtest/gtest.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

namespace nearby {
namespace linux_impl {
namespace {

class BluetoothAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char tmpl[] = "/tmp/qs_bt_adapter_XXXXXX";
    ASSERT_NE(mkdtemp(tmpl), nullptr) << strerror(errno);
    dir_ = std::string(tmpl);
  }

  void TearDown() override {
    Files::RemoveFile(FilePath(dir_ + "/name"));
    Files::RemoveFile(FilePath(dir_ + "/address"));
    Files::RemoveDirectory(FilePath(dir_));
  }

  void WriteAttribute(const std::string& name, const std::string& value) {
    std::ofstream file(dir_ + "/" + name);
    file << value;
  }

  std::string dir_;
};

TEST_F(BluetoothAdapterTest, ReportsDisabledWhenAdapterPathDoesNotExist) {
  BluetoothAdapter adapter(dir_ + "/missing");

  EXPECT_FALSE(adapter.IsEnabled());
  EXPECT_EQ(adapter.GetScanMode(), api::BluetoothAdapter::ScanMode::kNone);
  EXPECT_FALSE(adapter.SetStatus(api::BluetoothAdapter::Status::kEnabled));
}

TEST_F(BluetoothAdapterTest, ReadsNameAndAddressFromAdapterPath) {
  WriteAttribute("name", "Ubuntu Share\n");
  WriteAttribute("address", "01:23:45:67:89:AB\n");

  BluetoothAdapter adapter(dir_);

  EXPECT_TRUE(adapter.IsEnabled());
  EXPECT_EQ(adapter.GetName(), "Ubuntu Share");
  EXPECT_EQ(adapter.GetMacAddress().ToString(), "01:23:45:67:89:AB");
  EXPECT_EQ(adapter.GetScanMode(),
            api::BluetoothAdapter::ScanMode::kConnectable);
}

TEST_F(BluetoothAdapterTest, FallsBackToLinuxNameWhenNameMissing) {
  BluetoothAdapter adapter(dir_);

  EXPECT_EQ(adapter.GetName(), "Linux");
}

TEST_F(BluetoothAdapterTest, DoesNotPretendPrivilegedMutationsSucceeded) {
  BluetoothAdapter adapter(dir_);

  EXPECT_TRUE(adapter.SetStatus(api::BluetoothAdapter::Status::kEnabled));
  EXPECT_FALSE(adapter.SetStatus(api::BluetoothAdapter::Status::kDisabled));
  EXPECT_FALSE(adapter.SetName("Pixel Receiver"));
  EXPECT_TRUE(adapter.SetScanMode(
      api::BluetoothAdapter::ScanMode::kConnectableDiscoverable));
  EXPECT_FALSE(
      adapter.SetScanMode(api::BluetoothAdapter::ScanMode::kNone));
}

}  // namespace
}  // namespace linux_impl
}  // namespace nearby
