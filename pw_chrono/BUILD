# Copyright 2020 The Pigweed Authors
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

# TODO(pwbug/101): Need to add support for facades/backends to Bazel.
PW_CHRONO_SYSTEM_CLOCK_BACKEND = "//pw_chrono_stl:system_clock"

pw_cc_library(
    name = "epoch",
    hdrs = [
        "public/pw_chrono/epoch.h",
    ],
    includes = ["public"],
)

pw_cc_library(
    name = "system_clock_facade",
    hdrs = [
        "public/pw_chrono/internal/system_clock_macros.h",
        "public/pw_chrono/system_clock.h",
    ],
    includes = ["public"],
    srcs = [
        "system_clock.cc"
    ],
    deps = [
        ":epoch",
        PW_CHRONO_SYSTEM_CLOCK_BACKEND + "_headers",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "system_clock",
    deps = [
        ":system_clock_facade",
        PW_CHRONO_SYSTEM_CLOCK_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "system_clock_backend",
    deps = [
       PW_CHRONO_SYSTEM_CLOCK_BACKEND,
    ],
)

pw_cc_library(
    name = "simulated_system_clock",
    hdrs = [
        "public/pw_chrono/simulated_system_clock.h",
    ],
    deps = [
        ":system_clock",
        "//pw_sync:interrupt_spin_lock",
    ],
)

pw_cc_test(
    name = "simulated_system_clock_test",
    srcs = [
        "simulated_system_clock_test.cc",
    ],
    deps = [
        ":simulated_system_clock",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "system_clock_facade_test",
    srcs = [
        "system_clock_facade_test.cc",
        "system_clock_facade_test_c.c",
    ],
    deps = [
        ":system_clock",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)
