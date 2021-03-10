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

pw_cc_library(
    name = "id_headers",
    hdrs = [
        "public/pw_thread_freertos/id_inline.h",
        "public/pw_thread_freertos/id_native.h",
        "public_overrides/pw_thread_backend/id_inline.h",
        "public_overrides/pw_thread_backend/id_native.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
)

pw_cc_library(
    name = "id",
    deps = [
        ":id_headers",
        "//pw_thread:id_facade",
    ],
    # TODO(pwbug/317): This should depend on FreeRTOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "sleep_headers",
    hdrs = [
        "public/pw_thread_freertos/sleep_inline.h",
        "public_overrides/pw_thread_backend/sleep_inline.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
    deps = [
        "//pw_chrono:system_clock",
    ],
)

pw_cc_library(
    name = "sleep",
    srcs = [
        "sleep.cc",
    ],
    deps = [
        ":sleep_headers",
        "//pw_chrono_freertos:system_clock_headers",
        "//pw_assert",
        "//pw_chrono:system_clock",
        "//pw_thread:sleep_facade",
    ],
    # TODO(pwbug/317): This should depend on FreeRTOS but our third parties
    # currently do not have Bazel support.
)

# This target provides the FreeRTOS specific headers needs for thread creation.
pw_cc_library(
    name = "thread_headers",
    hdrs = [
        "public/pw_thread_freertos/context.h",
        "public/pw_thread_freertos/options.h",
        "public/pw_thread_freertos/config.h",
        "public/pw_thread_freertos/thread_inline.h",
        "public/pw_thread_freertos/thread_native.h",
        "public_overrides/pw_thread_backend/thread_inline.h",
        "public_overrides/pw_thread_backend/thread_native.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
    deps = [
        "//pw_assert",
        ":id",
        "//pw_thread:thread_headers",
    ],
    # TODO(pwbug/317): This should depend on FreeRTOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "thread",
    srcs = [
        "thread.cc",
    ],
    deps = [
        "//pw_assert",
        ":id",
        ":thread_headers",
    ],
    # TODO(pwbug/317): This should depend on FreeRTOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "dynamic_test_threads",
    deps = [
        "//pw_thread:thread_facade",
        "//pw_thread:test_threads_header",
        "//pw_chrono:system_clock",
        "//pw_thread:sleep",
    ],
    srcs = [
        "dynamic_test_threads.cc",
    ]
)

pw_cc_test(
    name = "dynamic_thread_backend_test",
    deps = [
        "//pw_thread:thread_facade_test",
        ":dynamic_test_threads",
    ]
)

pw_cc_library(
    name = "static_test_threads",
    deps = [
        "//pw_thread:thread_facade",
        "//pw_thread:test_threads_header",
        "//pw_chrono:system_clock",
        "//pw_thread:sleep",
    ],
    srcs = [
        "static_test_threads.cc",
    ]
)

pw_cc_test(
    name = "static_thread_backend_test",
    deps = [
        "//pw_thread:thread_facade_test",
        ":static_test_threads",
    ]
)

pw_cc_library(
    name = "yield_headers",
    hdrs = [
        "public/pw_thread_freertos/yield_inline.h",
        "public_overrides/pw_thread_backend/yield_inline.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
    # TODO(pwbug/317): This should depend on FreeRTOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "yield",
    deps = [
        ":yield_headers",
        "//pw_thread:yield_facade",
    ],
)

