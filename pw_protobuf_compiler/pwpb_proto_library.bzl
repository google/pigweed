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
"""Rule for generating C++ proto libraries using pw_protobuf."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//pw_protobuf_compiler/private:proto.bzl", "compile_proto", "proto_compiler_aspect")

# For Copybara use only
ADDITIONAL_PWPB_DEPS = []

# TODO: b/373693434 - The `oneof_callbacks` parameter is temporary to assist
# with migration.
def pwpb_proto_library(*, name, deps, oneof_callbacks = True, **kwargs):
    """A C++ proto library generated using pw_protobuf.

    Attributes:
      deps: proto_library targets for which to generate this library.
    """
    if oneof_callbacks:
        _pwpb_proto_library(
            name = name,
            protos = deps,
            deps = [
                Label("//pw_assert"),
                Label("//pw_containers:vector"),
                Label("//pw_preprocessor"),
                Label("//pw_protobuf"),
                Label("//pw_result"),
                Label("//pw_span"),
                Label("//pw_status"),
                Label("//pw_string:string"),
            ] + ADDITIONAL_PWPB_DEPS,
            **kwargs
        )
    else:
        _pwpb_legacy_oneof_proto_library(
            name = name,
            protos = deps,
            deps = [
                Label("//pw_assert"),
                Label("//pw_containers:vector"),
                Label("//pw_preprocessor"),
                Label("//pw_protobuf"),
                Label("//pw_result"),
                Label("//pw_span"),
                Label("//pw_status"),
                Label("//pw_string:string"),
            ] + ADDITIONAL_PWPB_DEPS,
            **kwargs
        )

_pwpb_proto_compiler_aspect = proto_compiler_aspect(
    ["pwpb.h"],
    "//pw_protobuf/py:plugin",
    ["--exclude-legacy-snake-case-field-name-enums", "--no-legacy-namespace", "--options-file={}"],
)

_pwpb_proto_library = rule(
    implementation = compile_proto,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
        ),
        "protos": attr.label_list(
            providers = [ProtoInfo],
            aspects = [_pwpb_proto_compiler_aspect],
        ),
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)

# TODO: b/373693434 - This aspect and its corresponding rule should be removed
# once oneof callback migration is complete.
_pwpb_legacy_oneof_compiler_aspect = proto_compiler_aspect(
    ["pwpb.h"],
    "//pw_protobuf/py:plugin",
    ["--exclude-legacy-snake-case-field-name-enums", "--no-oneof-callbacks", "--no-legacy-namespace", "--options-file={}"],
)

_pwpb_legacy_oneof_proto_library = rule(
    implementation = compile_proto,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
        ),
        "protos": attr.label_list(
            providers = [ProtoInfo],
            aspects = [_pwpb_legacy_oneof_compiler_aspect],
        ),
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
