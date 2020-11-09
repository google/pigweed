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
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_library(
    name = "spin_lock_headers",
    hdrs = [
        "public/pw_sync_stl/spin_lock_inline.h",
        "public/pw_sync_stl/spin_lock_native.h",
        "public_overrides/pw_sync_backend/spin_lock_inline.h",
        "public_overrides/pw_sync_backend/spin_lock_native.h",
    ],
    includes = [
        "public",
        "public_overrides",
    ],
)

pw_cc_library(
    name = "spin_lock",
    deps = [
        ":spin_lock_headers",
        "//pw_sync:spin_lock_facade",
        "//pw_sync:yield_core",
    ],
)
