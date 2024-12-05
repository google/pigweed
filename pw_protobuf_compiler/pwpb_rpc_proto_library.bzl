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
"""Rule for generating C++ proto RPC libraries using pw_protobuf."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load("//pw_protobuf_compiler/private:proto.bzl", "compile_proto", "proto_compiler_aspect")

def pwpb_rpc_proto_library(*, name, deps, pwpb_proto_library_deps, **kwargs):
    """A pwpb_rpc proto library target.

    Attributes:
      deps: proto_library targets for which to generate this library.
      pwpb_proto_library_deps: A pwpb_proto_library generated
        from the same proto_library. Required.
    """
    _pw_pwpb_rpc_proto_library(
        name = name,
        protos = deps,
        deps = [
            Label("//pw_protobuf"),
            Label("//pw_rpc"),
            Label("//pw_rpc/pwpb:client_api"),
            Label("//pw_rpc/pwpb:server_api"),
        ] + pwpb_proto_library_deps,
        **kwargs
    )

_pw_pwpb_rpc_proto_compiler_aspect = proto_compiler_aspect(
    ["rpc.pwpb.h"],
    "//pw_rpc/py:plugin_pwpb",
    ["--no-legacy-namespace"],
)

_pw_pwpb_rpc_proto_library = rule(
    implementation = compile_proto,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
        ),
        "protos": attr.label_list(
            providers = [ProtoInfo],
            aspects = [_pw_pwpb_rpc_proto_compiler_aspect],
        ),
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
