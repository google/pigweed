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
load("//pw_protobuf_compiler/private:proto.bzl", "compile_proto", "proto_compiler_aspect")

# TODO: b/234873954 - Enable unused variable check.
# buildifier: disable=unused-variable
def nanopb_proto_library(*, name, deps, tags = [], options = None, **kwargs):
    """A C++ proto library generated using nanopb.

    Attributes:
      deps: proto_library targets for which to generate this library.
    """

    # TODO(tpudlik): Find a way to get Nanopb to generate nested structs.
    # Otherwise add the manual tag to the resulting library, preventing it
    # from being built unless directly depended on.  e.g. The 'Pigweed'
    # message in
    # pw_protobuf/pw_protobuf_test_protos/full_test.proto will fail to
    # compile as it has a self referring nested message. According to
    # the docs
    # https://jpa.kapsi.fi/nanopb/docs/reference.html#proto-file-options
    # and https://github.com/nanopb/nanopb/issues/433 it seems like it
    # should be possible to configure nanopb to generate nested structs via
    # flags in .options files.
    #
    # One issue is nanopb doesn't silently ignore unknown options in .options
    # files so we can't share .options files with pwpb.
    extra_tags = ["manual"]
    _nanopb_proto_library(
        name = name,
        protos = deps,
        deps = [
            "@com_github_nanopb_nanopb//:nanopb",
            Label("//pw_assert"),
            Label("//pw_containers:vector"),
            Label("//pw_preprocessor"),
            Label("//pw_result"),
            Label("//pw_span"),
            Label("//pw_status"),
            Label("//pw_string:string"),
        ],
        tags = tags + extra_tags,
        **kwargs
    )

_nanopb_proto_compiler_aspect = proto_compiler_aspect(
    ["pb.h", "pb.c"],
    "@com_github_nanopb_nanopb//:protoc-gen-nanopb",
    [],
    {
        "_pb_h": attr.label(
            default = Label("@com_github_nanopb_nanopb//:pb.h"),
            allow_single_file = True,
        ),
    },
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
