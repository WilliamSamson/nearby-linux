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

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "libnearby_bridge.so",
    srcs = [
        "nearby_service_bridge.cc",
        "share_session_service.h",
    ],
    linkshared = True,
    visibility = ["//visibility:public"],
    deps = [
        "//sharing:nearby_sharing_service",
        "//sharing/internal/api/linux:sharing_platform_linux",
        "//internal/platform/implementation/linux",
        "@com_google_absl//absl/strings",
    ],
    copts = ["-fPIC"],
)
