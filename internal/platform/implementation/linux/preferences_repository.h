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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_PREFERENCES_REPOSITORY_H_
#define PLATFORM_IMPLEMENTATION_LINUX_PREFERENCES_REPOSITORY_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"

namespace nearby {
namespace linux_impl {

// File-backed JSON store. `base_path` is a directory that will be created on
// first write; the file inside is named `settings.json`.
class PreferencesRepository {
 public:
  explicit PreferencesRepository(FilePath base_path)
      : base_path_(base_path),
        file_path_(base_path.append(FilePath("settings.json"))) {}

  nlohmann::json LoadPreferences() ABSL_LOCKS_EXCLUDED(mutex_);
  bool SavePreferences(nlohmann::json preferences) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  const FilePath base_path_;
  const FilePath file_path_;
  nlohmann::json value_ ABSL_GUARDED_BY(mutex_) = nlohmann::json::object();
  absl::Mutex mutex_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_PREFERENCES_REPOSITORY_H_
