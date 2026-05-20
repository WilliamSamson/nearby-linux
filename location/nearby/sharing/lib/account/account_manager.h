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

#ifndef LOCATION_NEARBY_SHARING_LIB_ACCOUNT_ACCOUNT_MANAGER_H_
#define LOCATION_NEARBY_SHARING_LIB_ACCOUNT_ACCOUNT_MANAGER_H_

#include <optional>
#include <string>
#include <vector>
#include "absl/strings/string_view.h"

namespace nearby::sharing {

class AccountManager {
 public:
  struct Account {
    std::string id;
    std::string email;
    std::string given_name;
    std::string display_name;
    std::string picture_url;
  };

  class Observer {
   public:
     virtual ~Observer() = default;
     virtual void OnLoginSucceeded(absl::string_view account_id) = 0;
     virtual void OnLogoutSucceeded(absl::string_view account_id,
                                    bool credential_error) = 0;
  };

  virtual ~AccountManager() = default;
  virtual std::optional<Account> GetCurrentAccount() const {
    return std::nullopt;
  }
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
};

}  // namespace nearby::sharing

#endif  // LOCATION_NEARBY_SHARING_LIB_ACCOUNT_ACCOUNT_MANAGER_H_
