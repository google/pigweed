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
"""Toolchain configuration for Bazel."""

load("//actions:defs.bzl", _pw_cc_action_name_set = "pw_cc_action_name_set")
load(
    "//cc_toolchain/private:action_config.bzl",
    _pw_cc_action_config = "pw_cc_action_config",
    _pw_cc_tool = "pw_cc_tool",
)
load(
    "//cc_toolchain/private:cc_toolchain.bzl",
    _pw_cc_toolchain = "pw_cc_toolchain",
)
load(
    "//cc_toolchain/private:feature.bzl",
    _pw_cc_feature = "pw_cc_feature",
    _pw_cc_feature_set = "pw_cc_feature_set",
)
load(
    "//cc_toolchain/private:flag_set.bzl",
    _pw_cc_flag_group = "pw_cc_flag_group",
    _pw_cc_flag_set = "pw_cc_flag_set",
)

# All of the following are deprecated. Instead use their targets.
OBJ_COPY_ACTION_NAME = "@pw_toolchain//actions:objcopy_embed_data"
COV_ACTION_NAME = "@pw_toolchain//actions:llvm_cov"
OBJ_DUMP_ACTION_NAME = "@pw_toolchain//actions:objdump_embed_data"
STRIP_ACTION_NAME = "@pw_toolchain//actions:strip"

# All of the following are deprecated. Instead use their targets.
ALL_AR_ACTIONS = ["@pw_toolchain//actions:all_ar_actions"]
ALL_ASM_ACTIONS = ["@pw_toolchain//actions:all_asm_actions"]
ALL_C_COMPILER_ACTIONS = ["@pw_toolchain//actions:all_c_compiler_actions"]
ALL_COVERAGE_ACTIONS = ["@pw_toolchain//actions:all_coverage_actions"]
ALL_CPP_COMPILER_ACTIONS = ["@pw_toolchain//actions:all_cpp_compiler_actions"]
ALL_LINK_ACTIONS = ["@pw_toolchain//actions:all_link_actions"]
ALL_OBJCOPY_ACTIONS = [OBJ_COPY_ACTION_NAME]
ALL_OBJDUMP_ACTIONS = [OBJ_DUMP_ACTION_NAME]
ALL_STRIP_ACTIONS = ["@pw_toolchain//actions:all_strip_actions"]

pw_cc_action_name_set = _pw_cc_action_name_set

pw_cc_action_config = _pw_cc_action_config
pw_cc_tool = _pw_cc_tool

pw_cc_feature = _pw_cc_feature
pw_cc_feature_set = _pw_cc_feature_set

pw_cc_flag_group = _pw_cc_flag_group
pw_cc_flag_set = _pw_cc_flag_set

pw_cc_toolchain = _pw_cc_toolchain
