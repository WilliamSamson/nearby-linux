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

#include "internal/platform/implementation/linux/scheduled_executor.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"

namespace nearby {
namespace linux_impl {

// Atomic state for a scheduled task: tracks Cancel vs first-execution-wins.
class ScheduledExecutor::Cancelable : public api::Cancelable {
 public:
  bool Cancel() override {
    Status expected = kNotRun;
    return status_.compare_exchange_strong(expected, kCanceled);
  }

  // Returns true on first successful claim, false if the task was canceled
  // or already executed (which shouldn't happen with single-shot scheduling).
  bool MarkExecuted() {
    Status expected = kNotRun;
    return status_.compare_exchange_strong(expected, kExecuted);
  }

 private:
  enum Status { kNotRun, kExecuted, kCanceled };
  std::atomic<Status> status_{kNotRun};
};

ScheduledExecutor::ScheduledExecutor() : worker_pool_(1) {
  scheduler_ = std::thread(&ScheduledExecutor::SchedulerLoop, this);
}

ScheduledExecutor::~ScheduledExecutor() { Shutdown(); }

void ScheduledExecutor::Execute(Runnable&& runnable) {
  worker_pool_.Execute(std::move(runnable));
}

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration delay) {
  auto cancelable = std::make_shared<Cancelable>();
  if (shutdown_.load(std::memory_order_acquire)) return cancelable;
  absl::Time when = absl::Now() + delay;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.emplace(when, Entry{cancelable, std::move(runnable)});
  }
  cv_.notify_one();
  return cancelable;
}

void ScheduledExecutor::Shutdown() {
  if (shutdown_.exchange(true, std::memory_order_acq_rel)) return;
  cv_.notify_all();
  if (scheduler_.joinable()) {
    if (std::this_thread::get_id() == scheduler_.get_id()) {
      scheduler_.detach();
    } else {
      scheduler_.join();
    }
  }
  worker_pool_.Shutdown();
}

void ScheduledExecutor::SchedulerLoop() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (!shutdown_.load(std::memory_order_acquire)) {
    if (tasks_.empty()) {
      cv_.wait(lock, [this] {
        return shutdown_.load(std::memory_order_acquire) || !tasks_.empty();
      });
      continue;
    }
    auto next = tasks_.begin();
    absl::Time now = absl::Now();
    if (next->first <= now) {
      Entry entry = std::move(next->second);
      tasks_.erase(next);
      lock.unlock();
      if (entry.cancelable->MarkExecuted()) {
        worker_pool_.Execute(std::move(entry.runnable));
      }
      lock.lock();
      continue;
    }
    absl::Duration wait_for = next->first - now;
    cv_.wait_for(lock, absl::ToChronoNanoseconds(wait_for));
  }
}

}  // namespace linux_impl
}  // namespace nearby
