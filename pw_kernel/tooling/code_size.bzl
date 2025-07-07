# Copyright 2025 The Pigweed Authors
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
"""A rule for calculating the code size of a binary target.
"""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("//pw_build:compatibility.bzl", "incompatible_with_mcu")
load("//pw_toolchain/action:action_names.bzl", "PW_ACTION_NAMES")

def _code_size_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )
    readelf = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = PW_ACTION_NAMES.readelf,
    )

    output = ctx.actions.declare_file(ctx.attr.name + "_code_size.csv")

    binary_files = []
    for binary in ctx.attr.binaries:
        for file_info in binary[DefaultInfo].files.to_list():
            binary_files.append(file_info)
    cmd = """
        SECTIONS_TO_CHECK=(
            ".code"
            ".static_init_ram"
            ".vector_table"
        )

        for ELF_FILE in "$@"; do
            TOTAL_SIZE_BYTES=0
            for SECTION_NAME in "${{SECTIONS_TO_CHECK[@]}}"; do
                SECTION_SIZE_BYTES=0
                HEX_SIZE=$({readelf} -S "${{ELF_FILE}}" 2>/dev/null | grep -E "[[:space:]]+${{SECTION_NAME}}[[:space:]]+" | awk '{{print $7}}')
                if [ -n "$HEX_SIZE" ]; then
                    SECTION_SIZE_BYTES=$((0x$HEX_SIZE))
                else
                    SECTION_SIZE_BYTES=0
                fi

                TOTAL_SIZE_BYTES=$((TOTAL_SIZE_BYTES + SECTION_SIZE_BYTES))
            done
            echo "${{ELF_FILE}},$TOTAL_SIZE_BYTES" >> {output}
        done
    """.format(
        output = output.path,
        readelf = readelf,
    )

    ctx.actions.run_shell(
        inputs = depset(
            direct = binary_files,
            transitive = [
                cc_toolchain.all_files,
            ],
        ),
        outputs = [output],
        arguments = [f.path for f in binary_files],
        command = cmd,
        mnemonic = "CalculateCodeSize",
    )

    return [DefaultInfo(files = depset([output]))]

_code_size = rule(
    implementation = _code_size_impl,
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
    attrs = {
        "binaries": attr.label_list(
            doc = "Binary files to calculate size of",
            mandatory = True,
        ),
    },
    doc = "Save the code size of binaries to a csv.",
)

def code_size(name, binaries, **kwargs):
    if kwargs.get("target_compatible_with") == None:
        kwargs["target_compatible_with"] = incompatible_with_mcu()

    _code_size(
        name = name,
        binaries = binaries,
        **kwargs
    )
