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
    "pw_cc_action_config",
    "pw_cc_tool",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# This build file defines a complete set of tools for a LLVM-based toolchain.

exports_files(glob(["**"]))

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

pw_cc_tool(
    name = "ar_tool",
    tool = "//:bin/llvm-ar",
)

pw_cc_action_config(
    name = "ar",
    action_names = ["@pw_toolchain//actions:all_ar_actions"],
    implies = [
        "archiver_flags",
        "linker_param_file",
    ],
    tools = [":ar_tool"],
)

pw_cc_tool(
    name = "clang++_tool",
    tool = "//:bin/clang++",
    additional_files = glob([
        "include/**",
        "lib/clang/**/include/**",
    ]),
)

pw_cc_action_config(
    name = "clang++",
    action_names = ["@pw_toolchain//actions:all_cpp_compiler_actions"],
    tools = [":clang++_tool"],
)

pw_cc_tool(
    name = "clang_tool",
    tool = "//:bin/clang",
    additional_files = glob([
        "include/**",
        "lib/clang/**/include/**",
    ]),
)

pw_cc_action_config(
    name = "clang",
    action_names = [
        "@pw_toolchain//actions:all_asm_actions",
        "@pw_toolchain//actions:all_c_compiler_actions",
    ],
    tools = [":clang_tool"],
)

# This tool is actually just clang++ under the hood, we just enumerate this
# tool differently to specify a different set of additional files.
pw_cc_tool(
    name = "lld_tool",
    tool = "//:bin/clang++",
    additional_files = glob([
        "lib/**/*.a",
        "lib/**/*.so*",
    ]),
)

pw_cc_action_config(
    name = "lld",
    action_names = ["@pw_toolchain//actions:all_link_actions"],
    tools = [":lld_tool"],
)

pw_cc_tool(
    name = "llvm_cov_tool",
    tool = "//:bin/llvm-cov",
)

pw_cc_action_config(
    name = "llvm-cov",
    action_names = ["@pw_toolchain//actions:all_coverage_actions"],
    tools = [":llvm_cov_tool"],
)

pw_cc_tool(
    name = "llvm_objcopy_tool",
    tool = "//:bin/llvm-objcopy",
)

pw_cc_action_config(
    name = "llvm-objcopy",
    action_names = ["@pw_toolchain//actions:all_objcopy_actions"],
    tools = [":llvm_objcopy_tool"],
)

pw_cc_tool(
    name = "llvm_objdump_tool",
    tool = "//:bin/llvm-objdump",
)

pw_cc_action_config(
    name = "llvm-objdump",
    action_names = ["@pw_toolchain//actions:all_objdump_actions"],
    tools = [":llvm_objdump_tool"],
)

pw_cc_tool(
    name = "llvm_strip_tool",
    tool = "//:bin/llvm-strip",
)

pw_cc_action_config(
    name = "llvm-strip",
    action_names = ["@pw_toolchain//actions:all_strip_actions"],
    tools = [":llvm_strip_tool"],
)
