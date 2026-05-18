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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_CREDENTIAL_STORAGE_H_
#define PLATFORM_IMPLEMENTATION_LINUX_CREDENTIAL_STORAGE_H_

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace linux_impl {

// In-memory credential store. Production deployments that need credentials to
// survive process restarts should layer a file-backed implementation over this
// API; the receive-from-everyone MVP regenerates ephemeral keys per session
// and does not require persistence yet.
class CredentialStorage final : public api::CredentialStorage {
 public:
  using LocalCredential = ::nearby::internal::LocalCredential;
  using SharedCredential = ::nearby::internal::SharedCredential;
  using PublicCredentialType = ::nearby::presence::PublicCredentialType;
  using LocalCredentialKey = std::pair<std::string, std::string>;
  using PublicCredentialKey =
      std::tuple<std::string, std::string, PublicCredentialType>;

  CredentialStorage() = default;
  ~CredentialStorage() override = default;

  void SaveCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<LocalCredential>& private_credentials,
      const std::vector<SharedCredential>& public_credentials,
      PublicCredentialType public_credential_type,
      SaveCredentialsResultCallback callback) override;

  void UpdateLocalCredential(absl::string_view manager_app_id,
                             absl::string_view account_name,
                             nearby::internal::LocalCredential credential,
                             SaveCredentialsResultCallback callback) override;

  void GetLocalCredentials(const CredentialSelector& credential_selector,
                           GetLocalCredentialsResultCallback callback) override;

  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

 private:
  void SaveLocalCredentialsLocked(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<LocalCredential>& private_credentials)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::StatusOr<std::vector<LocalCredential>> GetLocalCredentialsLocked(
      const CredentialSelector& credential_selector)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  static LocalCredentialKey CreateLocalCredentialKey(
      absl::string_view manager_app_id, absl::string_view account_name) {
    return std::make_pair(std::string(manager_app_id),
                          std::string(account_name));
  }
  static PublicCredentialKey CreatePublicCredentialKey(
      absl::string_view manager_app_id, absl::string_view account_name,
      PublicCredentialType type) {
    return std::make_tuple(std::string(manager_app_id),
                           std::string(account_name), type);
  }

  mutable absl::Mutex mutex_;
  absl::flat_hash_map<LocalCredentialKey, std::vector<LocalCredential>>
      private_credentials_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<PublicCredentialKey, std::vector<SharedCredential>>
      public_credentials_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_CREDENTIAL_STORAGE_H_
