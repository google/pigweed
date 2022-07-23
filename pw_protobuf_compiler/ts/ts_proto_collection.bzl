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

"""Builds a proto collection."""

load("@npm//@bazel/typescript:index.bzl", "ts_library")
load("@build_bazel_rules_nodejs//:index.bzl", "js_library")
load("@//pw_protobuf_compiler/ts/codegen:template_replacement.bzl", "template_replacement")

def _lib(name, proto_library, js_proto_library):
    js_proto_library_name = Label(js_proto_library).name
    proto_root_dir = js_proto_library_name + "/" + js_proto_library_name + "_pb"
    template_name = name + "_template"
    template_replacement(
        name = template_name,
        descriptor_data = proto_library,
        proto_root_dir = proto_root_dir,
        output_file = "generated/ts_proto_collection.ts",
    )

    ts_library(
        name = name + "_lib",
        package_name = name,
        module_name = name,
        srcs = [template_name],
        deps = [
            js_proto_library,
            "@//pw_protobuf_compiler/ts:pw_protobuf_compiler",
            "@//pw_rpc/ts:packet_proto_tspb",
            "@npm//@types/google-protobuf",
            "@npm//@types/node",
            "@npm//base64-js",
        ],
    )

    native.filegroup(
        name = name + "_esm",
        srcs = [name + "_lib"],
        output_group = "es6_sources",
    )

def ts_proto_collection(name, proto_library, js_proto_library):
    _lib(name, proto_library, js_proto_library)
    js_library(
        name = name,
        package_name = "@pigweed/" + name + "/pw_protobuf_compiler",
        deps = [name + "_lib", name + "_esm"],
    )
