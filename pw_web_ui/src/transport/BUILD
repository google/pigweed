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

load("@npm//@bazel/typescript:index.bzl", "ts_library")
load("@npm//@bazel/jasmine:index.bzl", "jasmine_node_test")
load("//pw_web_ui:web_bundle.bzl", "web_bundle")
load("@npm//@bazel/karma:index.bzl", "karma_web_test")

ts_library(
    name = "device_transport_lib",
    srcs = [
        "device_transport.ts",
    ],
    deps = [
        "@npm//rxjs",
    ],
)

ts_library(
    name = "serial_mock_lib",
    srcs = [
        "serial_mock.ts",
    ],
    deps = [
        "//pw_web_ui/types:serial_lib",
        "@npm//jasmine",
        "@npm//@types/jasmine",
        "@npm//rxjs",
    ],
)

ts_library(
    name = "web_serial_transport_lib",
    srcs = [
        "web_serial_transport.ts",
    ],
    deps = [
        ":device_transport_lib",
        "//pw_web_ui/types:serial_lib",
        "@npm//rxjs",
    ],
)

ts_library(
    name = "web_serial_transport_lib_test",
    srcs = [
        "web_serial_transport_test.ts",
    ],
    deps = [
        ":serial_mock_lib",
        ":web_serial_transport_lib",
        "@npm//jasmine",
        "@npm//@types/jasmine",
        "@npm//rxjs",
    ],
)

web_bundle(
    name = "web_serial_transport_lib_test_bundle",
    deps = [
        ":web_serial_transport_lib_test",
    ],
    entry_point = "web_serial_transport_test.ts",
)


karma_web_test(
    name = "web_test",
    srcs = [
        ":web_serial_transport_lib_test_bundle"
    ],
)
