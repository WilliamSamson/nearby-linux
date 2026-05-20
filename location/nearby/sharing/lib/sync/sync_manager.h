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

#ifndef LOCATION_NEARBY_SHARING_LIB_SYNC_SYNC_MANAGER_H_
#define LOCATION_NEARBY_SHARING_LIB_SYNC_SYNC_MANAGER_H_

#include <functional>
#include <string>
#include <string_view>
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace nearby::sharing {

namespace sync {

class SyncBinding {
 public:
  enum SourceDeviceType {
    SOURCE_DEVICE_TYPE_UNKNOWN = 0,
    SOURCE_DEVICE_TYPE_PHONE,
    SOURCE_DEVICE_TYPE_TABLET,
    SOURCE_DEVICE_TYPE_LAPTOP,
    SOURCE_DEVICE_TYPE_CAR,
    SOURCE_DEVICE_TYPE_FOLDABLE,
    SOURCE_DEVICE_TYPE_XR,
  };

  void set_binding_id(const std::string& id) {}
  void set_binding_id(std::string_view id) {}
  void set_source_name(const std::string& name) {}
  void set_source_name(std::string_view name) {}
  void set_destination_directory(const std::string& dir) {}
  void set_destination_directory(std::string_view dir) {}
  void set_source_device_type(SourceDeviceType type) {}
};

}  // namespace sync

class SyncManager {
 public:
  SyncManager(void* identity_client, void* preference_manager) {}
  virtual ~SyncManager() = default;

  virtual void AsyncInitiateSyncBinding(
      std::function<void(absl::StatusOr<std::string>)> callback) {
    if (callback) callback(absl::UnimplementedError("Not implemented"));
  }
  virtual void AddSyncBinding(const sync::SyncBinding& binding) {}
};

}  // namespace nearby::sharing

#endif  // LOCATION_NEARBY_SHARING_LIB_SYNC_SYNC_MANAGER_H_
