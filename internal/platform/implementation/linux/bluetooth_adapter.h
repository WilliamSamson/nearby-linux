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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLUETOOTH_ADAPTER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLUETOOTH_ADAPTER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace linux_impl {

// Minimal Linux adapter implementation for the receiver-first path. It reports
// local adapter identity from sysfs and leaves privileged mutations to the
// future BlueZ D-Bus medium layer.
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  BluetoothAdapter();
  explicit BluetoothAdapter(std::string adapter_path);
  ~BluetoothAdapter() override = default;

  bool SetStatus(Status status) override;
  bool IsEnabled() const override;

  ScanMode GetScanMode() const override;
  bool SetScanMode(ScanMode scan_mode) override;

  std::string GetName() const override;
  bool SetName(absl::string_view name) override;
  bool SetName(absl::string_view name, bool persist) override;

  MacAddress GetMacAddress() const override;

 private:
  std::string ReadAttribute(absl::string_view name) const;

  std::string adapter_path_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLUETOOTH_ADAPTER_H_
