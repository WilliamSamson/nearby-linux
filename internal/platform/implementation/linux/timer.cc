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

#include "internal/platform/implementation/linux/timer.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

#include "absl/functional/any_invocable.h"

namespace nearby {
namespace linux_impl {

Timer::~Timer() { Stop(); }

bool Timer::Create(int delay, int interval,
                   absl::AnyInvocable<void()> callback) {
  if (delay < 0 || interval < 0 || callback == nullptr) return false;

  Stop();  // cancel any prior schedule before reusing

  {
    std::lock_guard<std::mutex> lock(mutex_);
    delay_ms_ = delay;
    interval_ms_ = interval;
    callback_ = std::move(callback);
    running_.store(true, std::memory_order_release);
  }

  worker_ = std::thread(&Timer::Run, this);
  return true;
}

bool Timer::Stop() {
  bool was_running = running_.exchange(false, std::memory_order_acq_rel);
  cv_.notify_all();
  if (worker_.joinable()) {
    // Refuse to deadlock if Stop() is called from within the callback (same
    // thread as the worker). In that case the worker will see running_=false
    // on the next loop iteration and exit naturally.
    if (std::this_thread::get_id() == worker_.get_id()) {
      worker_.detach();
    } else {
      worker_.join();
    }
  }
  return was_running;
}

void Timer::Run() {
  // Initial delay.
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(delay_ms_),
                 [this] { return !running_.load(std::memory_order_acquire); });
  }

  while (running_.load(std::memory_order_acquire)) {
    callback_();
    if (interval_ms_ == 0) break;
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(interval_ms_),
                 [this] { return !running_.load(std::memory_order_acquire); });
  }
}

}  // namespace linux_impl
}  // namespace nearby
