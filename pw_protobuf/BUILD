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

pw_cc_library(
    name = "config",
    hdrs = ["public/pw_protobuf/config.h"],
    includes = ["public"],
)

pw_cc_library(
    name = "pw_protobuf",
    srcs = [
        "decoder.cc",
        "encoder.cc",
        "find.cc",
    ],
    hdrs = [
        "public/pw_protobuf/codegen.h",
        "public/pw_protobuf/decoder.h",
        "public/pw_protobuf/encoder.h",
        "public/pw_protobuf/find.h",
        "public/pw_protobuf/serialized_size.h",
        "public/pw_protobuf/wire_format.h",
    ],
    includes = ["public"],
    deps = [
        ":config",
        "//pw_span",
        "//pw_status",
        "//pw_varint",
    ],
)

pw_cc_test(
    name = "decoder_test",
    srcs = ["decoder_test.cc"],
    deps = [
        ":pw_protobuf",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "encoder_test",
    srcs = ["encoder_test.cc"],
    deps = [
        ":pw_protobuf",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "find_test",
    srcs = ["find_test.cc"],
    deps = [
        ":pw_protobuf",
        "//pw_unit_test",
    ],
)

# TODO(frolv): Figure out how to integrate pw_protobuf codegen into Bazel.
filegroup(
    name = "codegen_test",
    srcs = [
        "codegen_test.cc",
    ],
)

# TODO(frolv): Figure out how to add facade tests to Bazel.
filegroup(
    name = "varint_size_test",
    srcs = [
        "varint_size_test.cc",
    ],
)

# TODO(frolv): Figure out what to do about size reports in Bazel.
filegroup(
    name = "size_reports",
    srcs = glob([
        "size_report/*.cc",
    ]),
)
