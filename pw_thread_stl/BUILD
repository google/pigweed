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
        "public/pw_thread_stl/id_inline.h",
        "public/pw_thread_stl/id_native.h",
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
)

pw_cc_library(
    name = "sleep_headers",
    hdrs = [
        "public/pw_thread_stl/sleep_inline.h",
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
    deps = [
        ":sleep_headers",
        "//pw_chrono:system_clock",
        "//pw_thread:sleep_facade",
    ],
)

pw_cc_library(
    name = "thread_headers",
    hdrs = [
        "public/pw_thread_stl/options.h",
        "public/pw_thread_stl/thread_inline.h",
        "public/pw_thread_stl/thread_native.h",
        "public_overrides/pw_thread_backend/thread_inline.h",
        "public_overrides/pw_thread_backend/thread_native.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
)

pw_cc_library(
    name = "thread",
    deps = [
        ":thread_headers",
        "//pw_thread:thread_facade",
    ],
)

pw_cc_library(
    name = "test_threads",
    deps = [
        "//pw_thread:thread_facade",
        "//pw_thread:test_threads_header",
    ],
    srcs = [
        "test_threads.cc",
    ]
)

pw_cc_test(
    name = "thread_backend_test",
    deps = [
        "//pw_thread:thread_facade_test",
        ":test_threads",
    ]
)

pw_cc_library(
    name = "yield_headers",
    hdrs = [
        "public/pw_thread_stl/yield_inline.h",
        "public_overrides/pw_thread_backend/yield_inline.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
)

pw_cc_library(
    name = "yield",
    deps = [
        ":yield_headers",
        "//pw_thread:yield_facade",
    ],
)
