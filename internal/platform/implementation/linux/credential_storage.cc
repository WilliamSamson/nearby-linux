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

#include "internal/platform/implementation/linux/credential_storage.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace linux_impl {

namespace {
using ::nearby::internal::IdentityType;

template <class Credential>
void FilterIdentityType(std::vector<Credential>& credentials,
                        IdentityType identity_type) {
  if (identity_type == IdentityType::IDENTITY_TYPE_UNSPECIFIED) return;
  credentials.erase(
      std::remove_if(credentials.begin(), credentials.end(),
                     [identity_type](const Credential& c) {
                       return c.identity_type() != identity_type;
                     }),
      credentials.end());
}
}  // namespace

void CredentialStorage::SaveCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<LocalCredential>& private_credentials,
    const std::vector<SharedCredential>& public_credentials,
    PublicCredentialType public_credential_type,
    SaveCredentialsResultCallback callback) {
  if (private_credentials.empty() && public_credentials.empty()) {
    std::move(callback.credentials_saved_cb)(
        absl::InvalidArgumentError("No credentials to save"));
    return;
  }
  absl::MutexLock lock(mutex_);
  if (!private_credentials.empty()) {
    SaveLocalCredentialsLocked(manager_app_id, account_name,
                               private_credentials);
  }
  if (!public_credentials.empty()) {
    PublicCredentialKey key = CreatePublicCredentialKey(
        manager_app_id, account_name, public_credential_type);
    public_credentials_[key] = public_credentials;
  }
  std::move(callback.credentials_saved_cb)(absl::OkStatus());
}

void CredentialStorage::SaveLocalCredentialsLocked(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<LocalCredential>& private_credentials) {
  LocalCredentialKey key =
      CreateLocalCredentialKey(manager_app_id, account_name);
  private_credentials_[key] = private_credentials;
}

void CredentialStorage::UpdateLocalCredential(
    absl::string_view manager_app_id, absl::string_view account_name,
    LocalCredential credential, SaveCredentialsResultCallback callback) {
  absl::MutexLock lock(mutex_);
  absl::StatusOr<std::vector<LocalCredential>> creds = GetLocalCredentialsLocked(
      CredentialSelector{.manager_app_id = std::string(manager_app_id),
                         .account_name = std::string(account_name),
                         .identity_type = IdentityType::IDENTITY_TYPE_UNSPECIFIED});
  std::vector<LocalCredential> updated =
      creds.ok() ? std::move(*creds) : std::vector<LocalCredential>{};
  auto it = std::find_if(
      updated.begin(), updated.end(),
      [&](const LocalCredential& c) { return c.id() == credential.id(); });
  if (it == updated.end()) {
    updated.push_back(std::move(credential));
  } else {
    *it = std::move(credential);
  }
  SaveLocalCredentialsLocked(manager_app_id, account_name, updated);
  std::move(callback.credentials_saved_cb)(absl::OkStatus());
}

void CredentialStorage::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {
  absl::MutexLock lock(mutex_);
  std::move(callback.credentials_fetched_cb)(
      GetLocalCredentialsLocked(credential_selector));
}

absl::StatusOr<std::vector<CredentialStorage::LocalCredential>>
CredentialStorage::GetLocalCredentialsLocked(
    const CredentialSelector& credential_selector) {
  LocalCredentialKey key = CreateLocalCredentialKey(
      credential_selector.manager_app_id, credential_selector.account_name);
  auto found = private_credentials_.find(key);
  if (found == private_credentials_.end()) {
    return absl::NotFoundError(absl::StrFormat("No private credentials for %v",
                                               credential_selector));
  }
  std::vector<LocalCredential> creds = found->second;
  FilterIdentityType(creds, credential_selector.identity_type);
  if (creds.empty()) {
    return absl::NotFoundError(absl::StrFormat("No private credentials for %v",
                                               credential_selector));
  }
  return creds;
}

void CredentialStorage::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  absl::MutexLock lock(mutex_);
  PublicCredentialKey key = CreatePublicCredentialKey(
      credential_selector.manager_app_id, credential_selector.account_name,
      public_credential_type);
  auto found = public_credentials_.find(key);
  if (found == public_credentials_.end()) {
    std::move(callback.credentials_fetched_cb)(absl::NotFoundError(
        absl::StrFormat("No public credentials for %v", credential_selector)));
    return;
  }
  std::vector<SharedCredential> creds = found->second;
  FilterIdentityType(creds, credential_selector.identity_type);
  if (creds.empty()) {
    std::move(callback.credentials_fetched_cb)(absl::NotFoundError(
        absl::StrFormat("No public credentials for %v", credential_selector)));
    return;
  }
  std::move(callback.credentials_fetched_cb)(std::move(creds));
}

}  // namespace linux_impl
}  // namespace nearby
