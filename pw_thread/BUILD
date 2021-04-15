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
PW_THREAD_ID_BACKEND = "//pw_thread_stl:id"
PW_THREAD_SLEEP_BACKEND = "//pw_thread_stl:sleep"
PW_THREAD_THREAD_BACKEND = "//pw_thread_stl:thread"
PW_THREAD_YIELD_BACKEND = "//pw_thread_stl:yield"

pw_cc_library(
    name = "id_facade",
    hdrs = [
        "public/pw_thread/id.h",
    ],
    includes = ["public"],
    deps = [
        PW_THREAD_ID_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "id",
    deps = [
        ":id_facade",
        PW_THREAD_ID_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "id_backend",
    deps = [
       PW_THREAD_ID_BACKEND,
    ],
)

pw_cc_library(
    name = "sleep_facade",
    hdrs = [
        "public/pw_thread/sleep.h",
    ],
    includes = ["public"],
    srcs = [
        "sleep.cc"
    ],
    deps = [
        PW_THREAD_SLEEP_BACKEND + "_headers",
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "sleep",
    deps = [
        ":sleep_facade",
        PW_THREAD_SLEEP_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "sleep_backend",
    deps = [
       PW_THREAD_SLEEP_BACKEND,
    ],
)

pw_cc_library(
    name = "thread_facade",
    hdrs = [
        "public/pw_thread/thread.h",
    ],
    includes = ["public"],
    deps = [
        ":id_facade",
        PW_THREAD_THREAD_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "thread",
    deps = [
        ":thread_facade",
        ":thread_core",
        PW_THREAD_THREAD_BACKEND + "_headers",
    ],
    srcs = [
        "thread.cc"
    ],
)

pw_cc_library(
    name = "thread_backend",
    deps = [
       PW_THREAD_THREAD_BACKEND,
    ],
)

pw_cc_library(
    name = "thread_core",
    hdrs = [
        "public/pw_thread/thread_core.h",
    ],
    includes = ["public"],
)

pw_cc_library(
    name = "yield_facade",
    hdrs = [
        "public/pw_thread/yield.h",
    ],
    includes = ["public"],
    srcs = [
        "yield.cc"
    ],
    deps = [
        PW_THREAD_YIELD_BACKEND + "_headers",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "yield",
    deps = [
        ":yield_facade",
        PW_THREAD_YIELD_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "yield_backend",
    deps = [
       PW_THREAD_YIELD_BACKEND,
    ],
)

pw_cc_library(
    name = "test_threads_header",
    hdrs = [
        "public/pw_thread/test_threads.h",
    ],
    deps = [
        ":thread",
    ],
)

# To instantiate this as a pw_cc_test, depend on this pw_cc_library and the
# pw_cc_library which implements the backend for test_threads_header. See
# //pw_thread:thread_backend_test as an example.
pw_cc_library(
    name = "thread_facade_test",
    srcs = [
        "thread_facade_test.cc",
    ],
    deps = [
        ":thread",
        ":id",
        ":test_threads_header",
        "//pw_chrono:system_clock",
        "//pw_sync:binary_semaphore",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "id_facade_test",
    srcs = [
        "id_facade_test.cc",
    ],
    deps = [
        ":id",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "sleep_facade_test",
    srcs = [
        "sleep_facade_test.cc",
        "sleep_facade_test_c.c",
    ],
    deps = [
        ":sleep",
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "yield_facade_test",
    srcs = [
        "yield_facade_test.cc",
        "yield_facade_test_c.c",
    ],
    deps = [
        ":yield",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)
