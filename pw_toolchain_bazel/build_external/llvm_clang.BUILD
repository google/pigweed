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
    tool = select({
        "@platforms//os:windows": "//:bin/llvm-ar.exe",
        "//conditions:default": "//:bin/llvm-ar",
    }),
)

pw_cc_tool(
    name = "llvm_libtool_darwin_tool",
    tool = "//:bin/llvm-libtool-darwin",
)

pw_cc_action_config(
    name = "ar",
    action_names = ["@pw_toolchain//actions:all_ar_actions"],
    # Unlike most legacy features required to compile code, these features
    # aren't enabled by default, and are instead only implied by the built-in
    # action configs. We imply the features here to match the behavior of the
    # built-in action configs so flags are actually passed to `ar`.
    implies = [
        "@pw_toolchain//features/legacy:archiver_flags",
        "@pw_toolchain//features/legacy:linker_param_file",
    ],
    tools = select({
        "@platforms//os:macos": [":llvm_libtool_darwin_tool"],
        "//conditions:default": [":ar_tool"],
    }),
)

pw_cc_tool(
    name = "clang++_tool",
    tool = select({
        "@platforms//os:windows": "//:bin/clang++.exe",
        "//conditions:default": "//:bin/clang++",
    }),
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
    tool = select({
        "@platforms//os:windows": "//:bin/clang.exe",
        "//conditions:default": "//:bin/clang",
    }),
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
    tool = select({
        "@platforms//os:windows": "//:bin/clang++.exe",
        "//conditions:default": "//:bin/clang++",
    }),
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
    tool = select({
        "@platforms//os:windows": "//:bin/llvm-cov.exe",
        "//conditions:default": "//:bin/llvm-cov",
    }),
)

pw_cc_action_config(
    name = "llvm-cov",
    action_names = ["@pw_toolchain//actions:all_coverage_actions"],
    tools = [":llvm_cov_tool"],
)

pw_cc_tool(
    name = "llvm_objcopy_tool",
    tool = select({
        "@platforms//os:windows": "//:bin/llvm-objcopy.exe",
        "//conditions:default": "//:bin/llvm-objcopy",
    }),
)

pw_cc_action_config(
    name = "llvm-objcopy",
    action_names = ["@pw_toolchain//actions:all_objcopy_actions"],
    tools = [":llvm_objcopy_tool"],
)

pw_cc_tool(
    name = "llvm_objdump_tool",
    tool = select({
        "@platforms//os:windows": "//:bin/llvm-objdump.exe",
        "//conditions:default": "//:bin/llvm-objdump",
    }),
)

pw_cc_action_config(
    name = "llvm-objdump",
    action_names = ["@pw_toolchain//actions:all_objdump_actions"],
    tools = [":llvm_objdump_tool"],
)

pw_cc_tool(
    name = "llvm_strip_tool",
    tool = select({
        "@platforms//os:windows": "//:bin/llvm-strip.exe",
        "//conditions:default": "//:bin/llvm-strip",
    }),
)

pw_cc_action_config(
    name = "llvm-strip",
    action_names = ["@pw_toolchain//actions:all_strip_actions"],
    tools = [":llvm_strip_tool"],
)
