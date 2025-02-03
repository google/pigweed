# Copyright 2024 The Pigweed Authors
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
"""Rule for generating raw C++ RPC proto libraries."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//pw_protobuf_compiler/private:proto.bzl", "compile_proto", "proto_compiler_aspect")

def raw_rpc_proto_library(*, name, deps, **kwargs):
    """A raw C++ RPC proto library."""
    _pw_raw_rpc_proto_library(
        name = name,
        protos = deps,
        deps = [
            Label("//pw_rpc"),
            Label("//pw_rpc/raw:client_api"),
            Label("//pw_rpc/raw:server_api"),
        ],
        **kwargs
    )

_pw_raw_rpc_proto_compiler_aspect = proto_compiler_aspect(
    ["raw_rpc.pb.h"],
    "//pw_rpc/py:plugin_raw",
    ["--no-legacy-namespace"],
)

_pw_raw_rpc_proto_library = rule(
    implementation = compile_proto,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
        ),
        "protos": attr.label_list(
            providers = [ProtoInfo],
            aspects = [_pw_raw_rpc_proto_compiler_aspect],
        ),
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
