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
    name = "simple_printing_event_handler",
    srcs = [
        "simple_printing_event_handler.cc",
    ],
    hdrs = [
        "public/pw_unit_test/event_handler.h",
        "public/pw_unit_test/simple_printing_event_handler.h",
    ],
    deps = ["//pw_preprocessor"],
)

pw_cc_library(
    name = "main",
    srcs = [
        "framework.cc",
        "main.cc",
    ],
    hdrs = [
        "public/gtest/gtest.h",
        "public/pw_unit_test/event_handler.h",
        "public/pw_unit_test/framework.h",
    ],
    deps = [
        ":simple_printing_event_handler",
        "//pw_preprocessor",
        "//pw_status",
    ],
)

pw_cc_test(
    name = "framework_test",
    srcs = ["framework_test.cc"],
)
