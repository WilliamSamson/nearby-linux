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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_ATOMIC_REFERENCE_H_
#define PLATFORM_IMPLEMENTATION_LINUX_ATOMIC_REFERENCE_H_

#include <atomic>
#include <cstdint>

#include "internal/platform/implementation/atomic_reference.h"

namespace nearby {
namespace linux_impl {

class AtomicUint32 final : public api::AtomicUint32 {
 public:
  explicit AtomicUint32(std::uint32_t value) : value_(value) {}
  ~AtomicUint32() override = default;

  std::uint32_t Get() const override {
    return value_.load(std::memory_order_acquire);
  }
  void Set(std::uint32_t value) override {
    value_.store(value, std::memory_order_release);
  }

 private:
  std::atomic<std::uint32_t> value_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_ATOMIC_REFERENCE_H_
