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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_ATOMIC_BOOLEAN_H_
#define PLATFORM_IMPLEMENTATION_LINUX_ATOMIC_BOOLEAN_H_

#include <atomic>

#include "internal/platform/implementation/atomic_boolean.h"

namespace nearby {
namespace linux_impl {

class AtomicBoolean final : public api::AtomicBoolean {
 public:
  explicit AtomicBoolean(bool initial_value) : value_(initial_value) {}
  ~AtomicBoolean() override = default;

  bool Get() const override { return value_.load(std::memory_order_acquire); }
  bool Set(bool value) override {
    return value_.exchange(value, std::memory_order_acq_rel);
  }

 private:
  std::atomic<bool> value_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_ATOMIC_BOOLEAN_H_
