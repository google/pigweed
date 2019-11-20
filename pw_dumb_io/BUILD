# Copyright 2019 The Pigweed Authors
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
    name = "facade",
    hdrs = ["public/pw_dumb_io/dumb_io.h"],
    deps = [
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "default_putget_bytes",
    srcs = ["dumb_io.cc"],
    deps = [
        ":facade",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "pw_dumb_io",
    deps = [
        ":facade",
        # For now, hard-code to depend on stdio until bazel build is updated
        # to support multiple target configurations.
        "//pw_dumb_io_stdio",
        "//pw_span",
        "//pw_status",
    ],
)
