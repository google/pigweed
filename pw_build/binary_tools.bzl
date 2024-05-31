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
"""Rules for processing binary executables."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@pw_toolchain//actions:providers.bzl", "ActionNameInfo")

def _pw_elf_to_bin_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)

    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )
    objcopy_path = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = ctx.attr._objcopy[ActionNameInfo].name,
    )

    ctx.actions.run_shell(
        inputs = depset(
            direct = [ctx.executable.elf_input],
            transitive = [
                cc_toolchain.all_files,
            ],
        ),
        outputs = [ctx.outputs.bin_out],
        command = "{objcopy} {args} {input} {output}".format(
            objcopy = objcopy_path,
            args = "-Obinary",
            input = ctx.executable.elf_input.path,
            output = ctx.outputs.bin_out.path,
        ),
    )

    return DefaultInfo(
        files = depset([ctx.outputs.bin_out] + ctx.files.elf_input),
        executable = ctx.outputs.bin_out,
    )

pw_elf_to_bin = rule(
    implementation = _pw_elf_to_bin_impl,
    doc = """Takes in an ELF executable and uses the toolchain objcopy tool to
    create a binary file, not containing any ELF headers. This can be used to
    create a bare-metal bootable image.
    """,
    attrs = {
        "bin_out": attr.output(mandatory = True),
        "elf_input": attr.label(mandatory = True, executable = True, cfg = "target"),
        "_objcopy": attr.label(
            default = "@pw_toolchain//actions:objcopy_embed_data",
            providers = [ActionNameInfo],
        ),
    },
    executable = True,
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
)
