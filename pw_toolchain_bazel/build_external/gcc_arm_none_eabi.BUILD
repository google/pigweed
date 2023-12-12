# Copyright 2023 The Pigweed Authors
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

load(
    "@pw_toolchain//cc_toolchain:defs.bzl",
    "ALL_AR_ACTIONS",
    "ALL_ASM_ACTIONS",
    "ALL_COVERAGE_ACTIONS",
    "ALL_CPP_COMPILER_ACTIONS",
    "ALL_C_COMPILER_ACTIONS",
    "ALL_LINK_ACTIONS",
    "ALL_OBJCOPY_ACTIONS",
    "ALL_OBJDUMP_ACTIONS",
    "ALL_STRIP_ACTIONS",
    "pw_cc_action_config",
    "pw_cc_tool",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# This build file defines a complete set of tools for an arm-none-eabi GNU
# binutils-based toolchain.

exports_files(glob(["**"]))

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

pw_cc_tool(
    name = "arm-none-eabi-ar_tool",
    tool = "//:bin/arm-none-eabi-ar",
)

pw_cc_action_config(
    name = "arm-none-eabi-ar",
    action_names = ALL_AR_ACTIONS,
    implies = [
        "archiver_flags",
        "linker_param_file",
    ],
    tools = [":arm-none-eabi-ar_tool"],
)

pw_cc_tool(
    name = "arm-none-eabi-g++_tool",
    tool = "//:bin/arm-none-eabi-g++",
    additional_files = glob([
        "**/*.spec",
        "**/*.specs",
        "arm-none-eabi/include/**",
        "lib/gcc/arm-none-eabi/*/include/**",
        "lib/gcc/arm-none-eabi/*/include-fixed/**",
        "libexec/**",
    ]),
)

pw_cc_action_config(
    name = "arm-none-eabi-g++",
    action_names = ALL_CPP_COMPILER_ACTIONS,
    tools = [":arm-none-eabi-g++_tool"],
)

pw_cc_tool(
    name = "arm-none-eabi-gcc_tool",
    tool = "//:bin/arm-none-eabi-gcc",
    additional_files = glob([
        "**/*.spec",
        "**/*.specs",
        "arm-none-eabi/include/**",
        "lib/gcc/arm-none-eabi/*/include/**",
        "lib/gcc/arm-none-eabi/*/include-fixed/**",
        "libexec/**",
    ]) + [
        "arm-none-eabi/bin/as",  # The assembler needs to be explicilty added.
    ],
)

pw_cc_action_config(
    name = "arm-none-eabi-gcc",
    action_names = ALL_ASM_ACTIONS + ALL_C_COMPILER_ACTIONS,
    tools = [":arm-none-eabi-gcc_tool"],
)

# This tool is actually just g++ under the hood, we just enumerate this
# tool differently to specify a different set of additional files.
pw_cc_tool(
    name = "arm-none-eabi-ld_tool",
    tool = "//:bin/arm-none-eabi-g++",
    additional_files = glob([
        "**/*.a",
        "**/*.ld",
        "**/*.o",
        "**/*.spec",
        "**/*.specs",
        "**/*.so",
        "libexec/**",
    ]),
)

pw_cc_action_config(
    name = "arm-none-eabi-ld",
    action_names = ALL_LINK_ACTIONS,
    tools = [":arm-none-eabi-ld_tool"],
)

pw_cc_tool(
    name = "arm-none-eabi-gcov_tool",
    tool = "//:bin/arm-none-eabi-gcov",
)

pw_cc_action_config(
    name = "arm-none-eabi-gcov",
    action_names = ALL_COVERAGE_ACTIONS,
    tools = [":arm-none-eabi-gcov_tool"],
)

pw_cc_tool(
    name = "arm-none-eabi-objcopy_tool",
    tool = "//:bin/arm-none-eabi-objcopy",
)

pw_cc_action_config(
    name = "arm-none-eabi-objcopy",
    action_names = ALL_OBJCOPY_ACTIONS,
    tools = [":arm-none-eabi-objcopy_tool"],
)

pw_cc_tool(
    name = "arm-none-eabi-objdump_tool",
    tool = "//:bin/arm-none-eabi-objdump",
)

pw_cc_action_config(
    name = "arm-none-eabi-objdump",
    action_names = ALL_OBJDUMP_ACTIONS,
    tools = [":arm-none-eabi-objdump_tool"],
)

pw_cc_tool(
    name = "arm-none-eabi-strip_tool",
    tool = "//:bin/arm-none-eabi-strip",
)

pw_cc_action_config(
    name = "arm-none-eabi-strip",
    action_names = ALL_STRIP_ACTIONS,
    tools = [":arm-none-eabi-strip_tool"],
)
