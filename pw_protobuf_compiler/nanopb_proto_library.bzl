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
"""Rule for generating C++ proto libraries using nanopb."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//pw_protobuf_compiler/private:proto.bzl", "compile_proto", "proto_compiler_aspect")

def nanopb_proto_library(*, name, deps, **kwargs):
    """A C++ proto library generated using nanopb.

    Attributes:
      deps: proto_library targets for which to generate this library.
    """

    _nanopb_proto_library(
        name = name,
        protos = deps,
        deps = [
            Label("@nanopb//:nanopb"),
        ],
        **kwargs
    )

def _custom_opt_for_library_include_format():
    """Return correctly set --library-include-format.

    When using nanopb_proto_library from a monorepo in which nanopb is not an
    external repository but just a build target within the main tree, the
    #include statements need to be relative to the root of that tree. Handle
    this case using --library-include-format.
    """
    pb_h = Label("@nanopb//:pb.h")
    if pb_h.workspace_root == "":
        # Monorepo case
        return "--library-include-format=#include \"{}/%s\"".format(pb_h.package)
    else:
        return "--library-include-format=#include \"%s\""

_nanopb_proto_compiler_aspect = proto_compiler_aspect(
    ["pb.h", "pb.c"],
    Label("@nanopb//:protoc-gen-nanopb"),
    [_custom_opt_for_library_include_format()],
)

_nanopb_proto_library = rule(
    implementation = compile_proto,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
        ),
        "protos": attr.label_list(
            providers = [ProtoInfo],
            aspects = [_nanopb_proto_compiler_aspect],
        ),
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
