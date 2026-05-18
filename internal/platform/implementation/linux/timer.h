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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_TIMER_H_
#define PLATFORM_IMPLEMENTATION_LINUX_TIMER_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "absl/functional/any_invocable.h"
#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace linux_impl {

// Each Timer owns one worker std::thread. Construction is cheap; callers that
// expect to fire many short-lived timers should batch through a shared
// ScheduledExecutor instead.
class Timer final : public api::Timer {
 public:
  Timer() = default;
  ~Timer() override;

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override;
  bool Stop() override;

 private:
  void Run();
  void StopLocked();  // requires mutex_ held

  std::thread worker_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> running_{false};
  int delay_ms_ = 0;
  int interval_ms_ = 0;
  absl::AnyInvocable<void()> callback_;
};

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_TIMER_H_
