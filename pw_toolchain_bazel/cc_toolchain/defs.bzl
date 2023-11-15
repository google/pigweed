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
    "//cc_toolchain/private:cc_toolchain.bzl",
    _OBJ_COPY_ACTION_NAME = "OBJ_COPY_ACTION_NAME",
    _OBJ_DUMP_ACTION_NAME = "OBJ_DUMP_ACTION_NAME",
    _pw_cc_toolchain = "pw_cc_toolchain",
)
load(
    "//cc_toolchain/private:flag_set.bzl",
    _pw_cc_flag_group = "pw_cc_flag_group",
    _pw_cc_flag_set = "pw_cc_flag_set",
)
load(
    "//cc_toolchain/private:toolchain_feature.bzl",
    _pw_cc_toolchain_feature = "pw_cc_toolchain_feature",
)
load(
    "//cc_toolchain/private:utils.bzl",
    _ALL_AR_ACTIONS = "ALL_AR_ACTIONS",
    _ALL_ASM_ACTIONS = "ALL_ASM_ACTIONS",
    _ALL_CPP_COMPILER_ACTIONS = "ALL_CPP_COMPILER_ACTIONS",
    _ALL_C_COMPILER_ACTIONS = "ALL_C_COMPILER_ACTIONS",
    _ALL_LINK_ACTIONS = "ALL_LINK_ACTIONS",
)

ALL_ASM_ACTIONS = _ALL_ASM_ACTIONS
ALL_C_COMPILER_ACTIONS = _ALL_C_COMPILER_ACTIONS
ALL_CPP_COMPILER_ACTIONS = _ALL_CPP_COMPILER_ACTIONS
ALL_LINK_ACTIONS = _ALL_LINK_ACTIONS
ALL_AR_ACTIONS = _ALL_AR_ACTIONS

# TODO(b/301004620): Remove when bazel 7 is released and this constant exists in
# ACTION_NAMES
OBJ_COPY_ACTION_NAME = _OBJ_COPY_ACTION_NAME
OBJ_DUMP_ACTION_NAME = _OBJ_DUMP_ACTION_NAME

pw_cc_flag_group = _pw_cc_flag_group
pw_cc_flag_set = _pw_cc_flag_set

pw_cc_toolchain = _pw_cc_toolchain

# TODO: b/309533028 - This is deprecated, and will soon be removed.
pw_cc_toolchain_feature = _pw_cc_toolchain_feature
