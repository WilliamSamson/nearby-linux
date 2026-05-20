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
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "paths",
    srcs = ["paths.cc"],
    hdrs = ["paths.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "atomic",
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform/implementation:types",
    ],
)

cc_library(
    name = "linux",
    srcs = [
        "ble_medium.cc",
        "bluetooth_adapter.cc",
        "bluez_dbus.cc",
        "credential_storage.cc",
        "crypto.cc",
        "device_info.cc",
        "multi_thread_executor.cc",
        "platform.cc",
        "preferences_manager.cc",
        "preferences_repository.cc",
        "scheduled_executor.cc",
        "system_clock.cc",
        "timer.cc",
        "wifi_lan_medium.cc",
    ],
    hdrs = [
        "ble_medium.h",
        "bluetooth_adapter.h",
        "bluez_dbus.h",
        "credential_storage.h",
        "device_info.h",
        "multi_thread_executor.h",
        "preferences_manager.h",
        "preferences_repository.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
        "timer.h",
        "wifi_lan_medium.h",
    ],
    defines = ["NO_WEBRTC"],
    linkopts = [
        "-pthread",
        "-lgio-2.0",
        "-lgobject-2.0",
        "-lglib-2.0",
    ],
    visibility = [
        "//app:__subpackages__",
        "//connections:__subpackages__",
        "//internal:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":atomic",
        ":paths",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/proto:credential_cc_proto",
        "//internal/proto:local_credential_cc_proto",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "//internal/platform/implementation/shared:posix_condition_variable",
        "//internal/platform/implementation/shared:posix_mutex",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@boringssl//:crypto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//src/google/protobuf/json",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "paths_test",
    srcs = ["paths_test.cc"],
    deps = [
        ":paths",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "device_info_test",
    srcs = ["device_info_test.cc"],
    deps = [
        ":linux",
        "//internal/platform/implementation:types",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "bluetooth_adapter_test",
    srcs = ["bluetooth_adapter_test.cc"],
    deps = [
        ":linux",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform/implementation:types",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "preferences_manager_test",
    srcs = ["preferences_manager_test.cc"],
    deps = [
        ":linux",
        "//internal/base:file_path",
        "//internal/base:files",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
