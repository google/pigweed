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
"""Embedded-friendly replacement for native.cc_proto_library."""

load("//pw_build:pigweed.bzl", "pw_cc_library")
load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load(
    "@rules_proto_grpc//:defs.bzl",
    "ProtoLibraryAspectNodeInfo",
    "ProtoPluginInfo",
    "proto_compile_aspect_attrs",
    "proto_compile_aspect_impl",
    "proto_compile_attrs",
    "proto_compile_impl",
)

# Create aspect for cc_proto_compile
cc_proto_compile_aspect = aspect(
    implementation = proto_compile_aspect_impl,
    provides = [ProtoLibraryAspectNodeInfo],
    attr_aspects = ["deps"],
    attrs = dict(
        proto_compile_aspect_attrs,
        _plugins = attr.label_list(
            doc = "List of protoc plugins to apply",
            providers = [ProtoPluginInfo],
            default = [
                Label("@pigweed//pw_protobuf:pw_cc_plugin"),
                Label("@pigweed//pw_rpc:pw_cc_plugin"),
            ],
        ),
        _prefix = attr.string(
            doc = "String used to disambiguate aspects when generating outputs",
            default = "cc_proto_compile_aspect",
        ),
    ),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

# Create compile rule to apply aspect
_rule = rule(
    implementation = proto_compile_impl,
    attrs = dict(
        proto_compile_attrs,
        deps = attr.label_list(
            mandatory = True,
            providers = [ProtoInfo, ProtoLibraryAspectNodeInfo],
            aspects = [cc_proto_compile_aspect],
        ),
        protos = attr.label_list(
            providers = [ProtoInfo],
            doc = "List of proto_library targets.",
        ),
    ),
)

# Create macro for converting attrs and passing to compile
def _cc_proto_compile(**kwargs):
    _rule(
        verbose_string = "{}".format(kwargs.get("verbose", 0)),
        **kwargs
    )

def pw_proto_library(**kwargs):
    """ Embedded friendly replacement for native.cc_proto_library

    This Protobuf implementation is designed to run on embedded
    computers. Because of this the cc API differs from the standard
    Protobuf cc plugin. The generated headers in this library are not a drop in
    replacement for the standard cc_proto_library.

    Args:
        **kwargs: Equivalent inputs to cc_proto_library
    """

    # Compile protos
    name_pb = kwargs.get("name") + "_pb"
    _cc_proto_compile(
        name = name_pb,
        # Forward deps and verbose tags to implementation
        **{k: v for (k, v) in kwargs.items() if k in ("deps", "verbose")}
    )

    # Create cc_library
    pw_cc_library(
        name = kwargs.get("name"),
        srcs = [name_pb],
        deps = [
            "@pigweed//pw_protobuf",
        ],
        includes = [name_pb],
        strip_include_prefix = ".",
        visibility = kwargs.get("visibility"),
        linkstatic = 1,
    )
