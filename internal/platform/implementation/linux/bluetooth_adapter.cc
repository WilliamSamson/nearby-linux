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

#include <fstream>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"

namespace nearby {
namespace linux_impl {

namespace {

constexpr char kDefaultAdapterPath[] = "/sys/class/bluetooth/hci0";

}  // namespace

BluetoothAdapter::BluetoothAdapter() : BluetoothAdapter(kDefaultAdapterPath) {}

BluetoothAdapter::BluetoothAdapter(std::string adapter_path)
    : adapter_path_(std::move(adapter_path)) {}

bool BluetoothAdapter::SetStatus(Status status) {
  // Powering adapters on/off is a privileged BlueZ D-Bus operation. For now we
  // only confirm the requested enabled state if an adapter is already present.
  return status == Status::kEnabled && IsEnabled();
}

bool BluetoothAdapter::IsEnabled() const {
  return Files::DirectoryExists(FilePath(adapter_path_));
}

api::BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  return IsEnabled() ? ScanMode::kConnectable : ScanMode::kNone;
}

bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  // Discoverability/connectability also belongs in the BlueZ D-Bus layer.
  if (!IsEnabled()) return false;
  return scan_mode == ScanMode::kConnectable ||
         scan_mode == ScanMode::kConnectableDiscoverable;
}

std::string BluetoothAdapter::GetName() const {
  std::string name = ReadAttribute("name");
  return name.empty() ? "Linux" : name;
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  return SetName(name, /*persist=*/false);
}

bool BluetoothAdapter::SetName(absl::string_view /*name*/, bool /*persist*/) {
  return false;
}

MacAddress BluetoothAdapter::GetMacAddress() const {
  MacAddress address;
  MacAddress::FromString(ReadAttribute("address"), address);
  return address;
}

std::string BluetoothAdapter::ReadAttribute(absl::string_view name) const {
  std::ifstream file(absl::StrCat(adapter_path_, "/", name));
  if (!file.is_open()) return "";

  std::string value;
  std::getline(file, value);
  return std::string(absl::StripAsciiWhitespace(value));
}

}  // namespace linux_impl
}  // namespace nearby
