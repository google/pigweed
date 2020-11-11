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

# TODO(pwbug/101): Need to add support for facades/backends to Bazel.
PW_INTERRUPT_CONTEXT_BACKEND = "//pw_interrupt_context_cortex_m:context_armv7m"

pw_cc_library(
    name = "context_facade",
    hdrs = [
        "public/pw_interrupt/context.h",
    ],
    includes = ["public"],
    srcs = [
        "context.cc"
    ],
    deps = [
        PW_INTERRUPT_CONTEXT_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "context",
    deps = [
        ":context_facade",
        PW_INTERRUPT_CONTEXT_BACKEND + "_headers",
    ],
)

pw_cc_library(
    name = "context_backend",
    deps = [
       PW_INTERRUPT_CONTEXT_BACKEND,
    ],
)
