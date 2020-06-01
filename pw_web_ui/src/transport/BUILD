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

package(default_visibility = ["//visibility:public"])

load("@npm_bazel_typescript//:index.bzl", "ts_library")
load("@npm_bazel_jasmine//:index.bzl", "jasmine_node_test")


ts_library(
    name = "transport_lib",
    srcs = [
        "web_serial_transport.ts",
    ],
    deps = [],
)

ts_library(
    name = "transport_lib_test",
    testonly = True,
    srcs = [
        "web_serial_transport_test.ts",
    ],
    deps = [
        ":transport_lib",
        "@npm//jasmine",
        "@npm//@types/jasmine",
    ],
)

jasmine_node_test(
    name = "test",
    srcs = [
        ":transport_lib_test",
    ],
    deps = [
        ":transport_lib",
    ],
)
