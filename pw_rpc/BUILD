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
    name = "pw_rpc",
    srcs = [
        "base_server_writer.cc",
        "public/pw_rpc/internal/base_server_writer.h",
        "server.cc",
        "service.cc",
    ],
    hdrs = [
        "public/pw_rpc/server.h",
        "public/pw_rpc/server_context.h",
        "public/pw_rpc/service.h",
        "public/pw_rpc/internal/channel.h",
        "public/pw_rpc/internal/server.h",
        "public/pw_rpc/internal/hash.h",
        # TODO(hepler): Only building the test version of the server for now.
        "test_impl/public_overrides/pw_rpc/internal/method.h",
    ],
    includes = [
        "public",
        "test_impl/public_overrides/",
    ],
    deps = [
        ":common",
        "//pw_assert",
        "//pw_log",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "common",
    srcs = [
        "channel.cc",
        "packet.cc",
    ],
    hdrs = [
        "public/pw_rpc/channel.h",
        "public/pw_rpc/internal/base_method.h",
        "public/pw_rpc/internal/call.h",
        "public/pw_rpc/internal/packet.h",
        "public/pw_rpc/server.h",
    ],
    includes = ["public"],
    visibility = ["//visibility:private"],
    deps = [
        "//pw_assert",
        "//pw_log",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "internal_test_utils",
    hdrs = ["pw_rpc_private/internal_test_utils.h"],
    visibility = ["//visibility:private"],
    deps = [
        ":common",
        "//pw_span",
    ],
)

# TODO(hepler): Cannot build nanopb-dependent code in Bazel at the moment. Need
# to determine how best to support Nanopb builds and protobuf generation.
filegroup(
    name = "nanopb",
    srcs = [
        "nanopb/codegen_test.cc",
        "nanopb/echo_service_test.cc",
        "nanopb/method.cc",
        "nanopb/method_test.cc",
        "nanopb/public/pw_rpc/echo_service_nanopb.h",
        "nanopb/public/pw_rpc/internal/service_method_traits.h",
        "nanopb/public/pw_rpc/test_method_context.h",
        "nanopb/public_overrides/pw_rpc/internal/method.h",
        "nanopb/service_method_traits_test.cc",
        "nanopb/test.pb.c",
        "nanopb/test.pb.h",
        "nanopb/test_rpc.pb.h",
    ],
)

pw_cc_test(
    name = "base_server_writer_test",
    srcs = [
        "base_server_writer_test.cc",
    ],
    deps = [
        ":internal_test_utils",
        ":pw_rpc",
    ],
)

pw_cc_test(
    name = "channel_test",
    srcs = ["channel_test.cc"],
    deps = [
        ":common",
        ":test_utils_test_server",
    ],
)

pw_cc_test(
    name = "packet_test",
    srcs = [
        "packet_test.cc",
    ],
    deps = [
        ":common",
    ],
)

proto_library(
    name = "packet_proto",
    srcs = [
        "pw_rpc_protos/packet.proto",
    ],
)

pw_cc_test(
    name = "server_test",
    srcs = [
        "server_test.cc",
    ],
    deps = [
        ":internal_test_utils",
        ":pw_rpc",
        "//pw_assert",
    ],
)
