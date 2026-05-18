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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_IMPLEMENTATION_LINUX_SINGLE_THREAD_EXECUTOR_H_

#include "internal/platform/implementation/linux/multi_thread_executor.h"

namespace nearby {
namespace linux_impl {

class SingleThreadExecutor final : public MultiThreadExecutor {
 public:
  SingleThreadExecutor() : MultiThreadExecutor(1) {}
  ~SingleThreadExecutor() override = default;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_SINGLE_THREAD_EXECUTOR_H_
