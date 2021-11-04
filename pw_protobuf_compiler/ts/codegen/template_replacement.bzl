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

"""Utility for generating TS code with string replacement."""

load("@build_bazel_rules_nodejs//:providers.bzl", "run_node")

def _template_replacement_impl(ctx):
    output_file = ctx.actions.declare_file(ctx.attr.output_file)
    descriptor_data = ctx.files.descriptor_data[0]
    template_file = ctx.files.template_file[0]

    run_node(
        ctx,
        executable = "_template_replacement_bin",
        inputs = [descriptor_data, template_file],
        outputs = [output_file],
        arguments = [
            "--template",
            template_file.path,
            "--descriptor_data",
            descriptor_data.path,
            "--output",
            output_file.path,
            "--proto_root_dir",
            ctx.attr.proto_root_dir,
        ],
    )

    return [DefaultInfo(files = depset([output_file]))]

template_replacement = rule(
    implementation = _template_replacement_impl,
    attrs = {
        "_template_replacement_bin": attr.label(
            executable = True,
            cfg = "exec",
            default = Label("@//pw_protobuf_compiler/ts/codegen:template_replacement_bin"),
        ),
        "descriptor_data": attr.label(
            allow_files = [".proto.bin"],
        ),
        "proto_root_dir": attr.string(mandatory = True),
        "output_file": attr.string(mandatory = True),
        "template_file": attr.label(
            allow_files = [".ts"],
            default = Label("@//pw_protobuf_compiler/ts:ts_proto_collection_template"),
        ),
    },
)
