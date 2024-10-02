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
"""Rules to convert a binary blob into a C++ library."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load(
    "//pw_build/bazel_internal:pigweed_internal.bzl",
    _compile_cc = "compile_cc",
)

CcBlobInfo = provider(
    "Input to pw_cc_blob_library",
    fields = {
        "alignas": "If present, the byte array is aligned as specified. The " +
                   "value of this argument is used verbatim in an alignas() " +
                   "specifier for the blob byte array.",
        "file_path": "The file path for the binary blob.",
        "linker_section": "If present, places the byte array in the specified " +
                          "linker section.",
        "symbol_name": "The C++ symbol for the byte array.",
    },
)

def _pw_cc_blob_info_impl(ctx):
    return [CcBlobInfo(
        symbol_name = ctx.attr.symbol_name,
        file_path = ctx.file.file_path,
        linker_section = ctx.attr.linker_section,
        alignas = ctx.attr.alignas,
    )]

pw_cc_blob_info = rule(
    implementation = _pw_cc_blob_info_impl,
    attrs = {
        "alignas": attr.string(default = ""),
        "file_path": attr.label(allow_single_file = True),
        "linker_section": attr.string(default = ""),
        "symbol_name": attr.string(),
    },
    provides = [CcBlobInfo],
)

def _pw_cc_blob_library_impl(ctx):
    # Python tool takes a json file with info about blobs to generate.
    blobs = []
    blob_paths = []
    for blob in ctx.attr.blobs:
        blob_info = blob[CcBlobInfo]
        blob_paths.append(blob_info.file_path)
        blob_dict = {
            "file_path": blob_info.file_path.path,
            "linker_section": blob_info.linker_section,
            "symbol_name": blob_info.symbol_name,
        }
        if (blob_info.alignas):
            blob_dict["alignas"] = blob_info.alignas
        blobs.append(blob_dict)
    blob_json = ctx.actions.declare_file(ctx.label.name + "_blob.json")
    ctx.actions.write(blob_json, json.encode(blobs))

    hdr = ctx.actions.declare_file(ctx.attr.out_header)
    src = ctx.actions.declare_file(ctx.attr.out_header.removesuffix(".h") + ".cc")

    if (not ctx.attr.namespace):
        fail("namespace required for pw_cc_blob_library")

    args = ctx.actions.args()
    args.add("--blob-file={}".format(blob_json.path))
    args.add("--namespace={}".format(ctx.attr.namespace))
    args.add("--header-include={}".format(ctx.attr.out_header))
    args.add("--out-header={}".format(hdr.path))
    args.add("--out-source={}".format(src.path))

    ctx.actions.run(
        inputs = depset(direct = blob_paths + [blob_json]),
        progress_message = "Generating cc blob library for %s" % (ctx.label.name),
        tools = [
            ctx.executable._generate_cc_blob_library,
        ],
        outputs = [hdr, src],
        executable = ctx.executable._generate_cc_blob_library,
        arguments = [args],
    )

    include_path = ctx.bin_dir.path

    # If workspace_root is set, this target is in an external workspace, so the
    # generated file will be located under workspace_root.
    if ctx.label.workspace_root:
        include_path += "/" + ctx.label.workspace_root

    # If target is not in root BUILD file of repo, append package name as that's
    # where the generated file will end up.
    if ctx.label.package:
        include_path += "/" + ctx.label.package

    return _compile_cc(
        ctx,
        [src],
        [hdr],
        deps = ctx.attr.deps,
        alwayslink = ctx.attr.alwayslink,
        includes = [include_path],
        defines = [],
    )

pw_cc_blob_library = rule(
    implementation = _pw_cc_blob_library_impl,
    doc = """Turns binary blobs into a C++ library of hard-coded byte arrays.

    The byte arrays are constant initialized and are safe to access at any time,
    including before main().

    Args:
        ctx: Rule context.
        blobs: A list of CcBlobInfo where each entry corresponds to a binary
               blob to be transformed from file to byte array. This is a
               required field. Blob fields include:

               symbol_name [required]: The C++ symbol for the byte array.

               file_path [required]: The file path for the binary blob.

               linker_section [optional]: If present, places the byte array
                in the specified linker section.

               alignas [optional]: If present, the byte array is aligned as
                specified. The value of this argument is used verbatim
                in an alignas() specifier for the blob byte array.

        out_header: The header file to generate. Users will include this file
                    exactly as it is written here to reference the byte arrays.

        namespace: The C++ namespace in which to place the generated blobs.
        alwayslink: Whether this library should always be linked.
    """,
    attrs = {
        "alwayslink": attr.bool(default = False),
        "blobs": attr.label_list(providers = [CcBlobInfo]),
        "deps": attr.label_list(default = [Label("//pw_preprocessor")]),
        "namespace": attr.string(),
        "out_header": attr.string(),
        "_generate_cc_blob_library": attr.label(
            default = Label("//pw_build/py:generate_cc_blob_library"),
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [CcInfo],
    fragments = ["cpp"],
    toolchains = ["@rules_python//python:exec_tools_toolchain_type"] + use_cpp_toolchain(),
)
