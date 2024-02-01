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
    "//cc_toolchain/private:feature_constraint.bzl",
    _pw_cc_feature_constraint = "pw_cc_feature_constraint",
)
load(
    "//cc_toolchain/private:flag_set.bzl",
    _pw_cc_flag_group = "pw_cc_flag_group",
    _pw_cc_flag_set = "pw_cc_flag_set",
)
load(
    "//cc_toolchain/private:unsafe_feature.bzl",
    _pw_cc_unsafe_feature = "pw_cc_unsafe_feature",
)

pw_cc_action_name_set = _pw_cc_action_name_set

pw_cc_action_config = _pw_cc_action_config
pw_cc_tool = _pw_cc_tool

pw_cc_feature = _pw_cc_feature
pw_cc_unsafe_feature = _pw_cc_unsafe_feature
pw_cc_feature_constraint = _pw_cc_feature_constraint
pw_cc_feature_set = _pw_cc_feature_set

pw_cc_flag_group = _pw_cc_flag_group
pw_cc_flag_set = _pw_cc_flag_set

pw_cc_toolchain = _pw_cc_toolchain

# TODO(b/322872628): Remove this.
# DO NOT USE. This is a temporary variable to allow users to migrate to
# action_config implies labels without breaking their build.
ARCHIVER_FLAGS = "archiver_flags"
LINKER_PARAM_FILE = "linker_param_file"
