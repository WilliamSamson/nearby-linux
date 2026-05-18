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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPLEMENTATION_LINUX_MULTI_THREAD_EXECUTOR_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace linux_impl {

// Java-style executor backed by `max_parallelism` std::thread workers and an
// unbounded FIFO queue. Tasks submitted before Shutdown() are guaranteed to
// run; tasks submitted after Shutdown() are dropped.
class MultiThreadExecutor : public api::SubmittableExecutor {
 public:
  explicit MultiThreadExecutor(int max_parallelism);
  ~MultiThreadExecutor() override;

  MultiThreadExecutor(const MultiThreadExecutor&) = delete;
  MultiThreadExecutor& operator=(const MultiThreadExecutor&) = delete;

  void Execute(Runnable&& runnable) override;
  bool DoSubmit(Runnable&& runnable) override;
  void Shutdown() override;

 private:
  void Worker();

  std::vector<std::thread> workers_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<Runnable> queue_;
  bool shutdown_ = false;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_MULTI_THREAD_EXECUTOR_H_
