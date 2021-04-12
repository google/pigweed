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
    name = "client",
    srcs = [
        "base_client_call.cc",
        "client.cc",
    ],
    hdrs = [
        "public/pw_rpc/client.h",
        "public/pw_rpc/internal/base_client_call.h",
    ],
    deps = [
        ":common",
    ],
)

pw_cc_library(
    name = "server",
    srcs = [
        "base_server_writer.cc",
        "public/pw_rpc/internal/base_server_writer.h",
        "public/pw_rpc/internal/call.h",
        "public/pw_rpc/internal/hash.h",
        "public/pw_rpc/internal/method.h",
        "public/pw_rpc/internal/method_lookup.h",
        "public/pw_rpc/internal/method_union.h",
        "public/pw_rpc/internal/server.h",
        "server.cc",
        "service.cc",
    ],
    hdrs = [
        "public/pw_rpc/server.h",
        "public/pw_rpc/server_context.h",
        "public/pw_rpc/service.h",
    ],
    includes = ["public"],
    deps = [
        ":common",
    ],
)

pw_cc_library(
    name = "client_server",
    srcs = ["client_server.cc"],
    hdrs = ["public/pw_rpc/client_server.h"],
    deps = [
        ":client",
        ":server",
    ],
)

pw_cc_library(
    name = "common",
    srcs = [
        "channel.cc",
        "packet.cc",
        "public/pw_rpc/internal/channel.h",
        "public/pw_rpc/internal/config.h",
        "public/pw_rpc/internal/method_type.h",
        "public/pw_rpc/internal/packet.h",
    ],
    hdrs = [
        "public/pw_rpc/channel.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_assert",
        "//pw_log",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "synchronized_channel_output",
    hdrs = ["public/pw_rpc/synchronized_channel_output.h"],
    includes = ["public"],
    deps = [
        ":common",
        "//pw_sync:lock_annotations",
        "//pw_sync:mutex",
    ],
)

pw_cc_library(
    name = "internal_test_utils",
    hdrs = [
        "public/pw_rpc/internal/test_method.h",
        "pw_rpc_private/internal_test_utils.h",
        "pw_rpc_private/method_impl_tester.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":server",
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
        "nanopb/method_lookup_test.cc",
        "nanopb/nanopb_client_call.cc",
        "nanopb/nanopb_client_call_test.cc",
        "nanopb/nanopb_common.cc",
        "nanopb/nanopb_method.cc",
        "nanopb/nanopb_method_test.cc",
        "nanopb/nanopb_method_union_test.cc",
        "nanopb/public/pw_rpc/echo_service_nanopb.h",
        "nanopb/public/pw_rpc/internal/nanopb_common.h",
        "nanopb/public/pw_rpc/internal/nanopb_method.h",
        "nanopb/public/pw_rpc/internal/nanopb_method_union.h",
        "nanopb/public/pw_rpc/nanopb_client_call.h",
        "nanopb/public/pw_rpc/nanopb_test_method_context.h",
        "nanopb/pw_rpc_nanopb_private/internal_test_utils.h",
        "nanopb/stub_generation_test.cc",
    ],
)

pw_cc_test(
    name = "base_server_writer_test",
    srcs = [
        "base_server_writer_test.cc",
    ],
    deps = [
        ":internal_test_utils",
        ":server",
    ],
)

pw_cc_test(
    name = "base_client_call_test",
    srcs = [
        "base_client_call_test.cc",
    ],
    deps = [
        ":client",
        ":internal_test_utils",
    ],
)

pw_cc_test(
    name = "client_test",
    srcs = [
        "client_test.cc",
    ],
    deps = [
        ":client",
        ":internal_test_utils",
    ],
)

pw_cc_test(
    name = "channel_test",
    srcs = ["channel_test.cc"],
    deps = [
        ":server",
        ":test_utils_test_server",
    ],
)

pw_cc_test(
    name = "packet_test",
    srcs = [
        "packet_test.cc",
    ],
    deps = [
        ":server",
    ],
)

pw_cc_test(
    name = "client_server_test",
    srcs = ["client_server_test.cc"],
    deps = [
        ":client_server",
        "//pw_rpc/raw:method_union",
    ],
)

proto_library(
    name = "packet_proto",
    srcs = [
        "pw_rpc_protos/internal/packet.proto",
    ],
)

pw_cc_test(
    name = "server_test",
    srcs = [
        "server_test.cc",
    ],
    deps = [
        ":internal_test_utils",
        ":server",
        "//pw_assert",
    ],
)

pw_cc_test(
    name = "service_test",
    srcs = [
        "service_test.cc",
    ],
    deps = [
        ":internal_test_utils",
        ":server",
        "//pw_assert",
    ],
)
