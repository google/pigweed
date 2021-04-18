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
    "pw_cc_facade",
    "pw_cc_library",
    "pw_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_facade(
    name = "binary_semaphore_facade",
    hdrs = [
        "public/pw_sync/binary_semaphore.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "binary_semaphore",
    srcs = [
        "binary_semaphore.cc",
    ],
    deps = [
        ":binary_semaphore_facade",
        "@pigweed_config//:pw_sync_binary_semaphore_backend",
    ],
)

pw_cc_library(
    name = "binary_semaphore_backend_multiplexer",
    visibility = ["@pigweed_config//:__pkg__"],
    deps = select({
        "@platforms//os:none": ["//pw_sync_baremetal:binary_semaphore"],
        "//pw_build/constraints/rtos:embos": ["//pw_sync_embos:binary_semaphore"],
        "//pw_build/constraints/rtos:freertos": ["//pw_sync_freertos:binary_semaphore"],
        "//pw_build/constraints/rtos:threadx": ["//pw_sync_threadx:binary_semaphore"],
        "//conditions:default": ["//pw_sync_stl:binary_semaphore"],
    }),
)

pw_cc_facade(
    name = "counting_semaphore_facade",
    hdrs = [
        "public/pw_sync/counting_semaphore.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "counting_semaphore",
    srcs = [
        "counting_semaphore.cc",
    ],
    deps = [
        ":counting_semaphore_facade",
        "@pigweed_config//:pw_sync_counting_semaphore_backend",
    ],
)

pw_cc_library(
    name = "counting_semaphore_backend_multiplexer",
    visibility = ["@pigweed_config//:__pkg__"],
    deps = select({
        "@platforms//os:none": ["//pw_sync_baremetal:counting_semaphore"],
        "//pw_build/constraints/rtos:embos": ["//pw_sync_embos:counting_semaphore"],
        "//pw_build/constraints/rtos:freertos": ["//pw_sync_freertos:counting_semaphore"],
        "//pw_build/constraints/rtos:threadx": ["//pw_sync_threadx:counting_semaphore"],
        "//conditions:default": ["//pw_sync_stl:counting_semaphore"],
    }),
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

pw_cc_facade(
    name = "mutex_facade",
    hdrs = [
        "public/pw_sync/mutex.h",
    ],
    includes = ["public"],
    deps = [
        ":lock_annotations",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "mutex",
    srcs = [
        "mutex.cc",
    ],
    deps = [
        ":mutex_facade",
        "@pigweed_config//:pw_sync_mutex_backend",
    ],
)

pw_cc_library(
    name = "mutex_backend_multiplexer",
    visibility = ["@pigweed_config//:__pkg__"],
    deps = select({
        "@platforms//os:none": ["//pw_sync_baremetal:mutex"],
        "//pw_build/constraints/rtos:embos": ["//pw_sync_embos:mutex"],
        "//pw_build/constraints/rtos:freertos": ["//pw_sync_freertos:mutex"],
        "//pw_build/constraints/rtos:threadx": ["//pw_sync_threadx:mutex"],
        "//conditions:default": ["//pw_sync_stl:mutex"],
    }),
)

pw_cc_facade(
    name = "timed_mutex_facade",
    hdrs = [
        "public/pw_sync/timed_mutex.h",
    ],
    includes = ["public"],
    deps = [
        ":lock_annotations",
        ":mutex_facade",
        "//pw_chrono:system_clock",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "timed_mutex",
    srcs = [
        "timed_mutex.cc",
    ],
    deps = [
        ":mutex",
        ":timed_mutex_facade",
        "@pigweed_config//:pw_sync_timed_mutex_backend",
    ],
)

pw_cc_library(
    name = "timed_mutex_backend_multiplexer",
    visibility = ["@pigweed_config//:__pkg__"],
    deps = select({
        "@platforms//os:none": ["//pw_sync_baremetal:timed_mutex"],
        "//pw_build/constraints/rtos:embos": ["//pw_sync_embos:timed_mutex"],
        "//pw_build/constraints/rtos:freertos": ["//pw_sync_freertos:timed_mutex"],
        "//pw_build/constraints/rtos:threadx": ["//pw_sync_threadx:timed_mutex"],
        "//conditions:default": ["//pw_sync_stl:timed_mutex"],
    }),
)

pw_cc_facade(
    name = "interrupt_spin_lock_facade",
    hdrs = [
        "public/pw_sync/interrupt_spin_lock.h",
    ],
    includes = ["public"],
    deps = [
        ":lock_annotations",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "interrupt_spin_lock",
    srcs = [
        "interrupt_spin_lock.cc",
    ],
    deps = [
        ":interrupt_spin_lock_facade",
        "@pigweed_config//:pw_sync_interrupt_spin_lock_backend",
    ],
)

pw_cc_library(
    name = "interrupt_spin_lock_backend_multiplexer",
    visibility = ["@pigweed_config//:__pkg__"],
    deps = select({
        "@platforms//os:none": ["//pw_sync_baremetal:interrupt_spin_lock"],
        "//pw_build/constraints/rtos:embos": ["//pw_sync_embos:interrupt_spin_lock"],
        "//pw_build/constraints/rtos:freertos": ["//pw_sync_freertos:interrupt_spin_lock"],
        "//pw_build/constraints/rtos:threadx": ["//pw_sync_threadx:interrupt_spin_lock"],
        "//conditions:default": ["//pw_sync_stl:interrupt_spin_lock"],
    }),
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
