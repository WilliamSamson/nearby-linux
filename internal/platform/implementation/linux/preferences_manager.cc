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

#include "internal/platform/implementation/linux/preferences_manager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/linux/preferences_repository.h"
#include "google/protobuf/json/json.h"
#include "google/protobuf/message.h"

namespace nearby {
namespace linux_impl {

namespace {
using json = ::nlohmann::json;
}  // namespace

PreferencesManager::PreferencesManager(FilePath preferences_dir) {
  repo_ = std::make_unique<PreferencesRepository>(preferences_dir);
  value_ = repo_->LoadPreferences();
}

bool PreferencesManager::Set(absl::string_view key, const json& value) {
  absl::MutexLock lock(mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetBoolean(absl::string_view key, bool value) {
  absl::MutexLock lock(mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetInteger(absl::string_view key, int value) {
  absl::MutexLock lock(mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetInt64(absl::string_view key, int64_t value) {
  absl::MutexLock lock(mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetString(absl::string_view key,
                                   absl::string_view value) {
  absl::MutexLock lock(mutex_);
  return SetValue(key, absl::StrCat(value));
}

bool PreferencesManager::SetBooleanArray(absl::string_view key,
                                         absl::Span<const bool> value) {
  absl::MutexLock lock(mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetIntegerArray(absl::string_view key,
                                         absl::Span<const int> value) {
  absl::MutexLock lock(mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetInt64Array(absl::string_view key,
                                       absl::Span<const int64_t> value) {
  absl::MutexLock lock(mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetStringArray(absl::string_view key,
                                        absl::Span<const std::string> value) {
  absl::MutexLock lock(mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetTime(absl::string_view key, absl::Time value) {
  absl::MutexLock lock(mutex_);
  int64_t nanos = absl::ToUnixNanos(value);
  if (value_[absl::StrCat(key)] == nanos) return false;
  value_[absl::StrCat(key)] = nanos;
  return Commit();
}

bool PreferencesManager::SetProtoMessage(
    absl::string_view key, const google::protobuf::Message& value) {
  std::string json_string;
  if (!google::protobuf::json::MessageToJsonString(value, &json_string).ok()) {
    return false;
  }
  absl::MutexLock lock(mutex_);
  return SetValue(key, json::parse(json_string));
}

json PreferencesManager::Get(absl::string_view key,
                             const json& default_value) const {
  absl::MutexLock lock(mutex_);
  return GetValue(key, default_value);
}

bool PreferencesManager::GetBoolean(absl::string_view key,
                                    bool default_value) const {
  absl::MutexLock lock(mutex_);
  return GetValue(key, default_value);
}

int PreferencesManager::GetInteger(absl::string_view key,
                                   int default_value) const {
  absl::MutexLock lock(mutex_);
  return GetValue(key, default_value);
}

int64_t PreferencesManager::GetInt64(absl::string_view key,
                                     int64_t default_value) const {
  absl::MutexLock lock(mutex_);
  return GetValue(key, default_value);
}

std::string PreferencesManager::GetString(
    absl::string_view key, const std::string& default_value) const {
  absl::MutexLock lock(mutex_);
  return GetValue(key, default_value);
}

std::vector<bool> PreferencesManager::GetBooleanArray(
    absl::string_view key, absl::Span<const bool> default_value) const {
  absl::MutexLock lock(mutex_);
  return GetArrayValue(key, default_value);
}

std::vector<int> PreferencesManager::GetIntegerArray(
    absl::string_view key, absl::Span<const int> default_value) const {
  absl::MutexLock lock(mutex_);
  return GetArrayValue(key, default_value);
}

std::vector<int64_t> PreferencesManager::GetInt64Array(
    absl::string_view key, absl::Span<const int64_t> default_value) const {
  absl::MutexLock lock(mutex_);
  return GetArrayValue(key, default_value);
}

std::vector<std::string> PreferencesManager::GetStringArray(
    absl::string_view key, absl::Span<const std::string> default_value) const {
  absl::MutexLock lock(mutex_);
  return GetArrayValue(key, default_value);
}

absl::Time PreferencesManager::GetTime(absl::string_view key,
                                       absl::Time default_value) const {
  absl::MutexLock lock(mutex_);
  auto result = value_.find(absl::StrCat(key));
  if (result == value_.end()) return default_value;
  return absl::FromUnixNanos(result->get<int64_t>());
}

bool PreferencesManager::GetProtoMessage(
    absl::string_view key, google::protobuf::Message* value) const {
  absl::MutexLock lock(mutex_);
  auto result = value_.find(absl::StrCat(key));
  if (result == value_.end()) return false;
  return google::protobuf::json::JsonStringToMessage(result->dump(), value)
      .ok();
}

void PreferencesManager::Remove(absl::string_view key) {
  absl::MutexLock lock(mutex_);
  value_.erase(absl::StrCat(key));
  Commit();
}

bool PreferencesManager::RemoveKeyPrefix(absl::string_view prefix) {
  absl::MutexLock lock(mutex_);
  auto it = value_.begin();
  while (it != value_.end()) {
    if (absl::StartsWith(it.key(), prefix)) {
      it = value_.erase(it);
    } else {
      ++it;
    }
  }
  return Commit();
}

bool PreferencesManager::Commit() { return repo_->SavePreferences(value_); }

bool PreferencesManager::SetValue(absl::string_view key, const json& value) {
  if (value_[absl::StrCat(key)] == value) return false;
  value_[absl::StrCat(key)] = value;
  return Commit();
}

template <typename T>
T PreferencesManager::GetValue(absl::string_view key,
                               const T& default_value) const {
  auto it = value_.find(absl::StrCat(key));
  if (it == value_.end()) return default_value;
  return it->get<T>();
}

template <typename T>
bool PreferencesManager::SetArrayValue(absl::string_view key,
                                       absl::Span<const T> value) {
  json array_value = json::array();
  for (const T& item : value) array_value.push_back(item);
  if (value_[absl::StrCat(key)] == array_value) return false;
  value_[absl::StrCat(key)] = array_value;
  return Commit();
}

template <typename T>
std::vector<T> PreferencesManager::GetArrayValue(
    absl::string_view key, absl::Span<const T> default_value) const {
  std::vector<T> result;
  auto array_value = value_.find(absl::StrCat(key));
  if (array_value == value_.end() || !array_value->is_array()) {
    for (const T& v : default_value) result.push_back(v);
    return result;
  }
  for (auto it = array_value->begin(); it != array_value->end(); ++it) {
    result.push_back(it->get<T>());
  }
  return result;
}

}  // namespace linux_impl
}  // namespace nearby
