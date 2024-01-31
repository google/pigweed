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
"""Private utilities and global variables."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    rules_cc_flag_set = "flag_set",
)

ALL_FILE_GROUPS = {
    "ar_files": ["@pw_toolchain//actions:all_ar_actions"],
    "as_files": ["@pw_toolchain//actions:all_asm_actions"],
    "compiler_files": ["@pw_toolchain//actions:all_compiler_actions"],
    "coverage_files": ["@pw_toolchain//actions:llvm_cov"],
    "dwp_files": [],
    "linker_files": ["@pw_toolchain//actions:all_link_actions"],
    "objcopy_files": ["@pw_toolchain//actions:objcopy_embed_data"],
    "strip_files": ["@pw_toolchain//actions:strip"],
}

def actionless_flag_set(flag_set_to_copy):
    """Copies a flag_set, stripping `actions`.

    Args:
        flag_set_to_copy: The base flag_set to copy.
    Returns:
        flag_set with empty `actions` list.
    """
    return rules_cc_flag_set(
        with_features = flag_set_to_copy.with_features,
        flag_groups = flag_set_to_copy.flag_groups,
    )

def to_untyped_flag_set(flag_set):
    return rules_cc_flag_set(
        actions = list(flag_set.actions),
        flag_groups = list(flag_set.flag_groups),
    )
