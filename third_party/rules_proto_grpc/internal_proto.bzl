# Copyright 2022 The Pigweed Authors
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

"""Backend implementation for 'pw_protobuf_compiler/proto.bzl'"""

# Apache License, Version 2.0, January 2004, http://www.apache.org/licenses/
# Adapted from: https://github.com/rules-proto-grpc/rules_proto_grpc/
# Files adapted:
#  - rules_proto_grpc/cpp/cpp_grpc_library.bzl
#  - rules_proto_grpc/cpp/cpp_grpc_compile.bzl
# These two files have been adapted for use in Pigweed and combined into this
# file.

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
load("@rules_proto_grpc//internal:filter_files.bzl", "filter_files")

# Create compile rule
def _proto_compiler_aspect(plugin_group, prefix):
    return aspect(
        implementation = proto_compile_aspect_impl,
        provides = [ProtoLibraryAspectNodeInfo],
        attr_aspects = ["deps"],
        attrs = dict(
            proto_compile_aspect_attrs,
            _plugins = attr.label_list(
                doc = "List of protoc plugins to apply",
                providers = [ProtoPluginInfo],
                default = plugin_group,
            ),
            _prefix = attr.string(
                doc = "String used to disambiguate aspects when generating \
outputs",
                default = prefix,
            ),
        ),
        toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
    )

def _proto_compiler_rule(plugin_group, aspect):
    return rule(
        implementation = proto_compile_impl,
        attrs = dict(
            proto_compile_attrs,
            _plugins = attr.label_list(
                doc = "List of protoc plugins to apply",
                providers = [ProtoPluginInfo],
                default = plugin_group,
            ),
            protos = attr.label_list(
                providers = [ProtoInfo],
                doc = "List of proto_library targets.",
            ),
            deps = attr.label_list(
                doc = "List of proto_library targets. Prefer protos.",
                aspects = [aspect],
            ),
        ),
        toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
    )

nanopb_compile_aspect = _proto_compiler_aspect(
    [Label("//pw_rpc:nanopb_plugin")],
    "nanopb_proto_compile_aspect",
)
nanopb_compile = _proto_compiler_rule(
    [Label("//pw_rpc:nanopb_plugin")],
    nanopb_compile_aspect,
)

pwpb_compile_aspect = _proto_compiler_aspect(
    [Label("@pigweed//pw_protobuf:pw_cc_plugin")],
    "pwpb_proto_compile_aspect",
)
pwpb_compile = _proto_compiler_rule(
    [Label("@pigweed//pw_protobuf:pw_cc_plugin")],
    pwpb_compile_aspect,
)

raw_rpc_compile_aspect = _proto_compiler_aspect(
    [Label("@pigweed//pw_rpc:pw_cc_plugin_raw")],
    "raw_rpc_proto_compile_aspect",
)
raw_rpc_compile = _proto_compiler_rule(
    [Label("@pigweed//pw_rpc:pw_cc_plugin_raw")],
    raw_rpc_compile_aspect,
)

nanopb_rpc_compile_aspect = _proto_compiler_aspect(
    [
        Label("@pigweed//pw_rpc:pw_cc_plugin_nanopb_rpc"),
        Label("//pw_rpc:nanopb_plugin"),
    ],
    "nanopb_rpc_proto_compile_aspect",
)
nanopb_rpc_compile = _proto_compiler_rule(
    [
        Label("@pigweed//pw_rpc:pw_cc_plugin_nanopb_rpc"),
        Label("//pw_rpc:nanopb_plugin"),
    ],
    nanopb_rpc_compile_aspect,
)

PLUGIN_INFO = {
    "nanopb": {
        "compiler": nanopb_compile,
        "deps": ["@com_github_nanopb_nanopb//:nanopb"],
        "has_srcs": True,
        # TODO: Find a way to get Nanopb to generate nested structs.
        # Otherwise add the manual tag to the resulting library,
        # preventing it from being built unless directly depended on.
        # e.g. The 'Pigweed' message in
        # pw_protobuf/pw_protobuf_test_protos/full_test.proto will fail to
        # compile as it has a self referring nested message. According to
        # the docs
        # https://jpa.kapsi.fi/nanopb/docs/reference.html#proto-file-options
        # and https://github.com/nanopb/nanopb/issues/433 it seams like it
        # should be possible to configure nanopb to generate nested structs.
        "additional_tags": ["manual"],
    },
    "nanopb_rpc": {
        "compiler": nanopb_rpc_compile,
        "deps": [
            "@com_github_nanopb_nanopb//:nanopb",
            "@pigweed//pw_rpc/nanopb:server_api",
            "@pigweed//pw_rpc/nanopb:client_api",
            "@pigweed//pw_rpc",
        ],
        "has_srcs": True,
        # See above todo.
        "additional_tags": ["manual"],
    },
    "pwpb": {
        "compiler": pwpb_compile,
        "deps": ["@pigweed//pw_protobuf"],
        "has_srcs": False,
        "additional_tags": [],
    },
    "raw_rpc": {
        "compiler": raw_rpc_compile,
        "deps": [
            "@pigweed//pw_rpc",
            "@pigweed//pw_rpc/raw:server_api",
            "@pigweed//pw_rpc/raw:client_api",
        ],
        "has_srcs": False,
        "additional_tags": [],
    },
}

def pw_proto_library(name, **kwargs):  # buildifier: disable=function-docstring
    for plugin_name, info in PLUGIN_INFO.items():
        name_pb = name + "_pb" + "." + plugin_name
        additional_tags = [
            tag
            for tag in info["additional_tags"]
            if tag not in kwargs.get("tags", [])
        ]
        info["compiler"](
            name = name_pb,
            tags = additional_tags,
            # Forward deps and verbose tags to implementation
            verbose = kwargs.get("verbose", 0),
            deps = kwargs.get("deps", []),
            protos = kwargs.get("protos", []),
        )

        # Filter files to sources and headers
        filter_files(
            name = name_pb + "_srcs",
            target = name_pb,
            extensions = ["c", "cc", "cpp", "cxx"],
            tags = additional_tags,
        )

        filter_files(
            name = name_pb + "_hdrs",
            target = name_pb,
            extensions = ["h"],
            tags = additional_tags,
        )

        # Cannot use pw_cc_library here as it will add cxxopts.
        # Note that the srcs attribute here is passed in as a DefaultInfo
        # object, which is not supported by pw_cc_library.
        native.cc_library(
            name = name + "." + plugin_name,
            hdrs = [name_pb + "_hdrs"],
            includes = [name_pb],
            alwayslink = kwargs.get("alwayslink"),
            copts = kwargs.get("copts", []),
            defines = kwargs.get("defines", []),
            srcs = [name_pb + "_srcs"] if info["has_srcs"] else [],
            deps = info["deps"],
            include_prefix = kwargs.get("include_prefix", ""),
            linkopts = kwargs.get("linkopts", []),
            linkstatic = kwargs.get("linkstatic", True),
            local_defines = kwargs.get("local_defines", []),
            nocopts = kwargs.get("nocopts", ""),
            visibility = kwargs.get("visibility"),
            tags = kwargs.get("tags", []) + additional_tags,
        )

    if "manual" in kwargs.get("tags", []):
        additional_tags = []
    else:
        additional_tags = ["manual"]

    # Combine all plugins into a single library.
    native.cc_library(
        name = name,
        deps = [
            name + "." + plugin_name
            for plugin_name in PLUGIN_INFO.keys()
        ],
        tags = kwargs.get("tags", []) + additional_tags,
        **{k: v for k, v in kwargs.items() if k not in ["deps", "protos"]}
    )
