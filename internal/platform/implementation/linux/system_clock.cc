// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

#include "internal/platform/implementation/system_clock.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"

namespace nearby {

void SystemClock::Init() {}

absl::Time SystemClock::ElapsedRealtime() { return absl::Now(); }

Exception SystemClock::Sleep(absl::Duration duration) {
  absl::SleepFor(duration);
  return {Exception::kSuccess};
}

}  // namespace nearby
