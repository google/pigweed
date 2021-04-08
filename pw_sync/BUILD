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
PW_SYNC_BINARY_SEMAPHORE_BACKEND = "//pw_sync_stl:binary_semaphore"
PW_SYNC_COUNTING_SEMAPHORE_BACKEND = "//pw_sync_stl:counting_semaphore"
PW_SYNC_MUTEX_BACKEND = "//pw_sync_stl:mutex"
PW_SYNC_TIMED_MUTEX_BACKEND = "//pw_sync_stl:timed_mutex"
PW_SYNC_INTERRUPT_SPIN_LOCK_BACKEND = "//pw_sync_stl:interrupt_spin_lock"

pw_cc_library(
    name = "binary_semaphore_facade",
    hdrs = [
        "public/pw_sync/binary_semaphore.h",
    ],
    includes = ["public"],
    srcs = [
        "binary_semaphore.cc"
    ],
    deps = [
        PW_SYNC_BINARY_SEMAPHORE_BACKEND + "_headers",
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "binary_semaphore",
    deps = [
        ":binary_semaphore_facade",
        PW_SYNC_BINARY_SEMAPHORE_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "binary_semaphore_backend",
    deps = [
       PW_SYNC_BINARY_SEMAPHORE_BACKEND,
    ],
)

pw_cc_library(
    name = "counting_semaphore_facade",
    hdrs = [
        "public/pw_sync/counting_semaphore.h",
    ],
    includes = ["public"],
    srcs = [
        "counting_semaphore.cc"
    ],
    deps = [
        PW_SYNC_COUNTING_SEMAPHORE_BACKEND + "_headers",
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "counting_semaphore",
    deps = [
        ":counting_semaphore_facade",
        PW_SYNC_COUNTING_SEMAPHORE_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "counting_semaphore_backend",
    deps = [
       PW_SYNC_COUNTING_SEMAPHORE_BACKEND,
    ],
)

pw_cc_library(
    name = "lock_annotations",
    hdrs = [
        "public/pw_sync/lock_annotations.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "mutex_facade",
    hdrs = [
        "public/pw_sync/mutex.h",
    ],
    includes = ["public"],
    srcs = [
        "mutex.cc"
    ],
    deps = [
		    ":lock_annotations",
        "//pw_preprocessor",
        PW_SYNC_MUTEX_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "mutex",
    deps = [
        ":mutex_facade",
        PW_SYNC_MUTEX_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "mutex_backend",
    deps = [
       PW_SYNC_MUTEX_BACKEND,
    ],
)

pw_cc_library(
    name = "timed_mutex_facade",
    hdrs = [
        "public/pw_sync/timed_mutex.h",
    ],
    includes = ["public"],
    srcs = [
        "timed_mutex.cc"
    ],
    deps = [
		    ":lock_annotations",
        ":mutex_facade",
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
        PW_SYNC_TIMED_MUTEX_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "timed_mutex",
    deps = [
        ":timed_mutex_facade",
        PW_SYNC_TIMED_MUTEX_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "timed_mutex_backend",
    deps = [
       PW_SYNC_TIMED_MUTEX_BACKEND,
    ],
)

pw_cc_library(
    name = "interrupt_spin_lock_facade",
    hdrs = [
        "public/pw_sync/interrupt_spin_lock.h",
    ],
    includes = ["public"],
    srcs = [
        "interrupt_spin_lock.cc"
    ],
    deps = [
		    ":lock_annotations",
        "//pw_preprocessor",
        PW_SYNC_INTERRUPT_SPIN_LOCK_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "interrupt_spin_lock",
    deps = [
        ":interrupt_spin_lock_facade",
        PW_SYNC_INTERRUPT_SPIN_LOCK_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "interrupt_spin_lock_backend",
    deps = [
       PW_SYNC_INTERRUPT_SPIN_LOCK_BACKEND,
    ],
)

pw_cc_library(
    name = "yield_core",
    hdrs = [
        "public/pw_sync/yield_core.h",
    ],
    includes = ["public"],
)

pw_cc_test(
    name = "binary_semaphore_facade_test",
    srcs = [
        "binary_semaphore_facade_test.cc",
        "binary_semaphore_facade_test_c.c",
    ],
    deps = [
        ":binary_semaphore",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "counting_semaphore_facade_test",
    srcs = [
        "counting_semaphore_facade_test.cc",
        "counting_semaphore_facade_test_c.c",
    ],
    deps = [
        ":counting_semaphore",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "mutex_facade_test",
    srcs = [
        "mutex_facade_test.cc",
        "mutex_facade_test_c.c",
    ],
    deps = [
        ":mutex",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "timed_mutex_facade_test",
    srcs = [
        "timed_mutex_facade_test.cc",
        "timed_mutex_facade_test_c.c",
    ],
    deps = [
        ":timed_mutex",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "interrupt_spin_lock_facade_test",
    srcs = [
        "interrupt_spin_lock_facade_test.cc",
        "interrupt_spin_lock_facade_test_c.c",
    ],
    deps = [
        ":interrupt_spin_lock",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)
