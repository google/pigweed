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
    "pw_cc_binary",
    "pw_cc_library",
    "pw_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_library(
    name = "support_armv7m",
    includes = ["public"],
    deps = [
        "//pw_preprocessor",
        "//pw_string",
        "//pw_log",
    ],
    hdrs = [ "public/pw_cpu_exception_cortex_m/cpu_state.h" ],
    srcs = [
        "cpu_state.cc",
        "pw_cpu_exception_cortex_m_private/cortex_m_constants.h",
    ],
)

pw_cc_library(
    name = "proto_dump_armv7m",
    deps = [
        ":support_armv7m",
        ":cpu_state_protos",
        "//pw_protobuf",
        "//pw_status",
        "//pw_stream",
    ],
    hdrs = ["public/pw_cpu_exception_cortex_m/proto_dump.h"],
    srcs = ["proto_dump.cc"],
)

proto_library(
    name = "cpu_state_protos",
    srcs = ["pw_cpu_exception_cortex_m_protos/cpu_state.proto"],
)

# TODO(pwbug/296): The *_armv7m libraries work on ARMv8-M, but needs some minor
# patches for complete correctness. Add *_armv8m targets that use the same files
# but provide preprocessor defines to enable/disable architecture specific code.
pw_cc_library(
    name = "cpu_exception_armv7m",
    deps = [
        ":proto_dump_armv7m",
        ":support_armv7m",
        # TODO(pwbug/101): Need to add support for facades/backends to Bazel.
        "//pw_cpu_exception",
        "//pw_preprocessor",
    ],
    srcs = [
        "entry.cc",
        "pw_cpu_exception_cortex_m_private/cortex_m_constants.h",
    ],
)

pw_cc_test(
    name = "cpu_exception_entry_test",
    srcs = [
        "exception_entry_test.cc",
    ],
    deps = [
        ":cpu_exception_armv7m",
    ],
)
