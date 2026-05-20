# Copyright 2026 The Quick Share Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "sharing_platform_linux",
    srcs = [
        "sharing_platform_linux.cc",
    ],
    hdrs = [
        "sharing_platform_linux.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//sharing/internal/api:platform",
        "//internal/platform/implementation/linux",
        "//internal/platform/implementation/linux:paths",
        "//location/nearby/sharing/lib/account:account_manager",
        "@com_google_absl//absl/strings",
    ],
)
