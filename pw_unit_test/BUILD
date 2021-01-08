# Copyright 2019 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

load(
    "//pw_build:pigweed.bzl",
    "pw_cc_library",
    "pw_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_library(
    name = "pw_unit_test",
    srcs = [
        "framework.cc",
    ],
    hdrs = [
        "public/pw_unit_test/event_handler.h",
        "public/pw_unit_test/framework.h",
        "public_overrides/gtest/gtest.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
    deps = [
        "//pw_polyfill",
        "//pw_preprocessor",
        "//pw_string",
    ],
)

pw_cc_library(
    name = "simple_printing_event_handler",
    srcs = ["simple_printing_event_handler.cc"],
    hdrs = [
        "public/pw_unit_test/simple_printing_event_handler.h",
    ],
    includes = [
        "public",
    ],
    deps = [
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

filegroup(
    name = "logging_event_handler",
    srcs = [
        "logging_event_handler.cc",
        "public/pw_unit_test/logging_event_handler.h",
    ],
)

filegroup(
    name = "logging_main",
    srcs = [
        "logging_main.cc",
    ],
)

pw_cc_library(
    name = "main",
    srcs = [
        "simple_printing_main.cc",
    ],
    deps = [
        ":pw_unit_test",
        ":simple_printing_event_handler",
        "//pw_span",
        "//pw_sys_io",
    ],
)

pw_cc_library(
    name = "rpc_service",
    hdrs = [
        "public/pw_unit_test/internal/rpc_event_handler.h",
        "public/pw_unit_test/unit_test_service.h",
    ],
    srcs = [
        "rpc_event_handler.cc",
        "unit_test_service.cc",
    ],
    deps = [
        ":pw_unit_test",
        "//pw_log",
    ],
)

pw_cc_library(
    name = "rpc_main",
    srcs = [
        "rpc_main.cc",
    ],
    deps = [
        ":pw_unit_test",
        ":rpc_service",
        "//pw_hdlc:pw_rpc",
        "//pw_log",
        "//pw_rpc:server",
    ],
)

pw_cc_test(
    name = "framework_test",
    srcs = ["framework_test.cc"],
    deps = [
        ":pw_unit_test",
    ],
)
