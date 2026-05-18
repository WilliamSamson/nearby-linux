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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_SCHEDULED_EXECUTOR_H_
#define PLATFORM_IMPLEMENTATION_LINUX_SCHEDULED_EXECUTOR_H_

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/linux/multi_thread_executor.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace linux_impl {

class ScheduledExecutor final : public api::ScheduledExecutor {
 public:
  ScheduledExecutor();
  ~ScheduledExecutor() override;

  ScheduledExecutor(const ScheduledExecutor&) = delete;
  ScheduledExecutor& operator=(const ScheduledExecutor&) = delete;

  void Execute(Runnable&& runnable) override;
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration delay) override;
  void Shutdown() override;

 private:
  class Cancelable;

  struct Entry {
    std::shared_ptr<Cancelable> cancelable;
    Runnable runnable;
  };

  void SchedulerLoop();

  std::atomic<bool> shutdown_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  std::multimap<absl::Time, Entry> tasks_;
  MultiThreadExecutor worker_pool_;
  std::thread scheduler_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_SCHEDULED_EXECUTOR_H_
