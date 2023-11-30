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
    action_names = ALL_AR_ACTIONS,
    implies = [
        "archiver_flags",
        "linker_param_file",
    ],
    tools = [":ar_tool"],
)

pw_cc_tool(
    name = "clang++_tool",
    tool = "//:bin/clang++",
)

pw_cc_action_config(
    name = "clang++",
    action_names = ALL_CPP_COMPILER_ACTIONS,
    tools = [":clang++_tool"],
)

pw_cc_tool(
    name = "clang_tool",
    tool = "//:bin/clang",
)

pw_cc_action_config(
    name = "clang",
    action_names = ALL_ASM_ACTIONS + ALL_C_COMPILER_ACTIONS,
    tools = [":clang_tool"],
)

pw_cc_action_config(
    name = "lld",
    action_names = ALL_LINK_ACTIONS,
    tools = [":clang++_tool"],  # Use the clang++ frontend to drive lld.
)

pw_cc_tool(
    name = "llvm_cov_tool",
    tool = "//:bin/llvm-cov",
)

pw_cc_action_config(
    name = "llvm-cov",
    action_names = ALL_COVERAGE_ACTIONS,
    tools = [":llvm_cov_tool"],
)

pw_cc_tool(
    name = "llvm_objcopy_tool",
    tool = "//:bin/llvm-objcopy",
)

pw_cc_action_config(
    name = "llvm-objcopy",
    action_names = ALL_OBJCOPY_ACTIONS,
    tools = [":llvm_objcopy_tool"],
)

pw_cc_tool(
    name = "llvm_objdump_tool",
    tool = "//:bin/llvm-objdump",
)

pw_cc_action_config(
    name = "llvm-objdump",
    action_names = ALL_OBJDUMP_ACTIONS,
    tools = [":llvm_objdump_tool"],
)

pw_cc_tool(
    name = "llvm_strip_tool",
    tool = "//:bin/llvm-strip",
)

pw_cc_action_config(
    name = "llvm-strip",
    action_names = ALL_STRIP_ACTIONS,
    tools = [":llvm_strip_tool"],
)
