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
"""Rule for generating a C++ RPC proto library using nanopb."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load("//pw_protobuf_compiler/private:proto.bzl", "compile_proto", "proto_compiler_aspect")

def nanopb_rpc_proto_library(*, name, deps, nanopb_proto_library_deps, tags = [], **kwargs):
    """A C++ RPC proto library using nanopb.

    Attributes:
      deps: proto_library targets for which to generate this library.
      nanopb_proto_library_deps: A nanopb_proto_library generated
        from the same proto_library. Required.
    """

    # See comment in nanopb_proto_library.
    extra_tags = ["manual"]
    _pw_nanopb_rpc_proto_library(
        name = name,
        protos = deps,
        # TODO: b/339280821 - This is required to avoid breaking internal
        # Google builds but shouldn't matter for any external user. Remove this
        # when possible.
        features = ["-layering_check"],
        deps = [
            Label("//pw_rpc"),
            Label("//pw_rpc/nanopb:client_api"),
            Label("//pw_rpc/nanopb:server_api"),
        ] + nanopb_proto_library_deps,
        tags = tags + extra_tags,
        **kwargs
    )

_pw_nanopb_rpc_proto_compiler_aspect = proto_compiler_aspect(
    ["rpc.pb.h"],
    "//pw_rpc/py:plugin_nanopb",
    ["--no-legacy-namespace"],
)

_pw_nanopb_rpc_proto_library = rule(
    implementation = compile_proto,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
        ),
        "protos": attr.label_list(
            providers = [ProtoInfo],
            aspects = [_pw_nanopb_rpc_proto_compiler_aspect],
        ),
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
