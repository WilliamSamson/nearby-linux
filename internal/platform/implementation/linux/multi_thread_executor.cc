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

#include "internal/platform/implementation/linux/multi_thread_executor.h"

#include <mutex>
#include <thread>
#include <utility>

#include "internal/platform/runnable.h"

namespace nearby {
namespace linux_impl {

MultiThreadExecutor::MultiThreadExecutor(int max_parallelism) {
  if (max_parallelism < 1) max_parallelism = 1;
  workers_.reserve(max_parallelism);
  for (int i = 0; i < max_parallelism; ++i) {
    workers_.emplace_back(&MultiThreadExecutor::Worker, this);
  }
}

MultiThreadExecutor::~MultiThreadExecutor() { Shutdown(); }

void MultiThreadExecutor::Execute(Runnable&& runnable) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_) return;
    queue_.push(std::move(runnable));
  }
  cv_.notify_one();
}

bool MultiThreadExecutor::DoSubmit(Runnable&& runnable) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_) return false;
    queue_.push(std::move(runnable));
  }
  cv_.notify_one();
  return true;
}

void MultiThreadExecutor::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_) return;
    shutdown_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) t.join();
  }
  workers_.clear();
}

void MultiThreadExecutor::Worker() {
  for (;;) {
    Runnable task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return shutdown_ || !queue_.empty(); });
      if (queue_.empty()) return;  // shutdown && empty -> exit
      task = std::move(queue_.front());
      queue_.pop();
    }
    task();
  }
}

}  // namespace linux_impl
}  // namespace nearby
