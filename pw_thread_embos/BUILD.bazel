# Copyright 2021 The Pigweed Authors
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
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

pw_cc_library(
    name = "id_headers",
    hdrs = [
        "public/pw_thread_embos/id_inline.h",
        "public/pw_thread_embos/id_native.h",
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
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "sleep_headers",
    hdrs = [
        "public/pw_thread_embos/sleep_inline.h",
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
        "//pw_assert",
        "//pw_chrono:system_clock",
        "//pw_chrono_embos:system_clock_headers",
        "//pw_thread:sleep_facade",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)

# This target provides the embOS specific headers needs for thread creation.
pw_cc_library(
    name = "thread_headers",
    hdrs = [
        "public/pw_thread_embos/config.h",
        "public/pw_thread_embos/context.h",
        "public/pw_thread_embos/options.h",
        "public/pw_thread_embos/thread_inline.h",
        "public/pw_thread_embos/thread_native.h",
        "public_overrides/pw_thread_backend/thread_inline.h",
        "public_overrides/pw_thread_backend/thread_native.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
    deps = [
        ":id",
        "//pw_assert",
        "//pw_string",
        "//pw_thread:thread_headers",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "thread",
    srcs = [
        "thread.cc",
    ],
    deps = [
        ":id",
        ":thread_headers",
        "//pw_assert",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "test_threads",
    srcs = [
        "test_threads.cc",
    ],
    deps = [
        "//pw_chrono:system_clock",
        "//pw_thread:sleep",
        "//pw_thread:test_threads_header",
        "//pw_thread:thread_facade",
    ],
)

pw_cc_library(
    name = "yield_headers",
    hdrs = [
        "public/pw_thread_embos/yield_inline.h",
        "public_overrides/pw_thread_backend/yield_inline.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "yield",
    deps = [
        ":yield_headers",
        "//pw_thread:yield_facade",
    ],
)

pw_cc_library(
    name = "util",
    srcs = [
        "util.cc",
    ],
    hdrs = [
        "public/pw_thread_embos/util.h",
    ],
    deps = [
        "//pw_function",
        "//pw_status",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)

pw_cc_library(
    name = "snapshot",
    srcs = [
        "snapshot.cc",
    ],
    hdrs = [
        "public/pw_thread_embos/snapshot.h",
    ],
    deps = [
        ":util",
        "//pw_bytes",
        "//pw_function",
        "//pw_log",
        "//pw_protobuf",
        "//pw_status",
        "//pw_thread:protos",
        "//pw_thread:snapshot",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
)
