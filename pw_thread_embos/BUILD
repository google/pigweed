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

licenses(["notice"])  # Apache License 2.0

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
        "//pw_chrono_embos:system_clock_headers",
        "//pw_assert",
        "//pw_chrono:system_clock",
        "//pw_thread:sleep_facade",
    ],
    # TODO(pwbug/317): This should depend on embOS but our third parties
    # currently do not have Bazel support.
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
