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

load(
    "@bazel_tools//tools/build_defs/cc:action_names.bzl",
    _STRIP_ACTION_NAME = "STRIP_ACTION_NAME",
)
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
load(
    "//cc_toolchain/private:utils.bzl",
    _ALL_AR_ACTIONS = "ALL_AR_ACTIONS",
    _ALL_ASM_ACTIONS = "ALL_ASM_ACTIONS",
    _ALL_CPP_COMPILER_ACTIONS = "ALL_CPP_COMPILER_ACTIONS",
    _ALL_C_COMPILER_ACTIONS = "ALL_C_COMPILER_ACTIONS",
    _ALL_LINK_ACTIONS = "ALL_LINK_ACTIONS",
    _LLVM_COV = "LLVM_COV",
    _OBJ_COPY_ACTION_NAME = "OBJ_COPY_ACTION_NAME",
    _OBJ_DUMP_ACTION_NAME = "OBJ_DUMP_ACTION_NAME",
)

# TODO(b/301004620): Remove when bazel 7 is released and these constants exists
# in ACTION_NAMES.
OBJ_COPY_ACTION_NAME = _OBJ_COPY_ACTION_NAME
COV_ACTION_NAME = _LLVM_COV

# This action name isn't yet a well-known action name.
OBJ_DUMP_ACTION_NAME = _OBJ_DUMP_ACTION_NAME

STRIP_ACTION_NAME = _STRIP_ACTION_NAME

ALL_AR_ACTIONS = _ALL_AR_ACTIONS
ALL_ASM_ACTIONS = _ALL_ASM_ACTIONS
ALL_C_COMPILER_ACTIONS = _ALL_C_COMPILER_ACTIONS
ALL_COVERAGE_ACTIONS = [COV_ACTION_NAME]
ALL_CPP_COMPILER_ACTIONS = _ALL_CPP_COMPILER_ACTIONS
ALL_LINK_ACTIONS = _ALL_LINK_ACTIONS
ALL_OBJCOPY_ACTIONS = [OBJ_COPY_ACTION_NAME]
ALL_OBJDUMP_ACTIONS = [OBJ_DUMP_ACTION_NAME]
ALL_STRIP_ACTIONS = [STRIP_ACTION_NAME]

pw_cc_action_config = _pw_cc_action_config
pw_cc_tool = _pw_cc_tool

pw_cc_feature = _pw_cc_feature
pw_cc_feature_set = _pw_cc_feature_set

pw_cc_flag_group = _pw_cc_flag_group
pw_cc_flag_set = _pw_cc_flag_set

pw_cc_toolchain = _pw_cc_toolchain
