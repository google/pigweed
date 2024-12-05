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
"""A filegroup grouping .proto and .options files."""

PwProtoOptionsInfo = provider(
    "Allows `pw_proto_filegroup` targets to pass along `.options` files " +
    "without polluting the `DefaultInfo` provider, which means they can " +
    "still be used in the `srcs` of `proto_library` targets.",
    fields = {
        "options_files": (".options file(s) associated with a proto_library " +
                          "for Pigweed codegen."),
    },
)

def _pw_proto_filegroup_impl(ctx):
    source_files = list()
    options_files = list()

    for src in ctx.attr.srcs:
        source_files += src.files.to_list()

    for options_src in ctx.attr.options_files:
        for file in options_src.files.to_list():
            if file.extension == "options" or file.extension == "pwpb_options":
                options_files.append(file)
            else:
                fail((
                    "Files provided as `options_files` to a " +
                    "`pw_proto_filegroup` must have the `.options` or " +
                    "`.pwpb_options` extensions; the file `{}` was provided."
                ).format(file.basename))

    return [
        DefaultInfo(files = depset(source_files)),
        PwProtoOptionsInfo(options_files = depset(options_files)),
    ]

pw_proto_filegroup = rule(
    doc = (
        "Acts like a `filegroup`, but with an additional `options_files` " +
        "attribute that accepts a list of `.options` files. These `.options` " +
        "files should typically correspond to `.proto` files provided under " +
        "the `srcs` attribute." +
        "\n\n" +
        "A `pw_proto_filegroup` is intended to be passed into the `srcs` of " +
        "a `proto_library` target as if it were a normal `filegroup` " +
        "containing only `.proto` files. For the purposes of the " +
        "`proto_library` itself, the `pw_proto_filegroup` does indeed act " +
        "just like a normal `filegroup`; the `options_files` attribute is " +
        "ignored. However, if that `proto_library` target is then passed " +
        "(directly or transitively) into the `deps` of a `pw_proto_library` " +
        "for code generation, the `pw_proto_library` target will have access " +
        "to the provided `.options` files and will pass them to the code " +
        "generator." +
        "\n\n" +
        "Note that, in order for a `pw_proto_filegroup` to be a valid `srcs` " +
        "entry for a `proto_library`, it must meet the same conditions " +
        "required of a standard `filegroup` in that context. Namely, its " +
        "`srcs` must provide at least one `.proto` (or `.protodevel`) file. " +
        "Put simply, a `pw_proto_filegroup` cannot be used as a vector for " +
        "injecting solely `.options` files; it must contain at least one " +
        "proto as well (generally one associated with an included `.options` " +
        "file in the interest of clarity)." +
        "\n\n" +
        "Regarding the somewhat unusual usage, this feature's design was " +
        "mostly preordained by the combination of Bazel's strict access " +
        "controls, the restrictions imposed on inputs to the `proto_library` " +
        "rule, and the need to support `.options` files from transitive " +
        "dependencies."
    ),
    implementation = _pw_proto_filegroup_impl,
    attrs = {
        "options_files": attr.label_list(
            allow_files = True,
        ),
        "srcs": attr.label_list(
            allow_files = True,
        ),
    },
    provides = [PwProtoOptionsInfo],
)
