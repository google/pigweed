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

load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES", "STRIP_ACTION_NAME")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "flag_set",
)

# A potentially more complete set of these constants are available at
# @rules_cc//cc:action_names.bzl, but it's not clear if they should be depended
# on.
ALL_ASM_ACTIONS = [
    ACTION_NAMES.assemble,
    ACTION_NAMES.preprocess_assemble,
]
ALL_C_COMPILER_ACTIONS = [
    ACTION_NAMES.c_compile,
    ACTION_NAMES.cc_flags_make_variable,
]
ALL_CPP_COMPILER_ACTIONS = [
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.cpp_header_parsing,
]
ALL_LINK_ACTIONS = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]
ALL_AR_ACTIONS = [
    ACTION_NAMES.cpp_link_static_library,
]

ACTION_MAP = {
    "aropts": ALL_AR_ACTIONS,
    "asmopts": ALL_ASM_ACTIONS,
    "copts": ALL_C_COMPILER_ACTIONS + ALL_CPP_COMPILER_ACTIONS,
    "conlyopts": ALL_C_COMPILER_ACTIONS,
    "cxxopts": ALL_CPP_COMPILER_ACTIONS,
    "linkopts": ALL_LINK_ACTIONS,
}

# TODO(b/301004620): Remove when bazel 7 is released and these constants exists
# in ACTION_NAMES.
LLVM_COV = "llvm-cov"
OBJ_COPY_ACTION_NAME = "objcopy_embed_data"

# This action name isn't yet a well-known action name.
OBJ_DUMP_ACTION_NAME = "objdump_embed_data"

# A dictionary mapping the `*_files` attribute names of a `cc_toolchain` rule
# to their respective applicable actions.
#
# `all_files` is intentionally omitted from this, as it applies to all actions.
ALL_FILE_GROUPS = {
    "ar_files": ALL_AR_ACTIONS,
    "as_files": ALL_ASM_ACTIONS,
    "compiler_files": ALL_C_COMPILER_ACTIONS + ALL_CPP_COMPILER_ACTIONS,
    "coverage_files": [LLVM_COV],
    "dwp_files": [],
    "linker_files": ALL_LINK_ACTIONS,
    "objcopy_files": [OBJ_COPY_ACTION_NAME],
    "strip_files": [STRIP_ACTION_NAME],
}

def check_deps_provide(ctx, list_name, provider, what_provides):
    """Ensures that each dep in the specified list offers the required provider.

    Args:
        ctx: The rule context to pull the dependency list from.
        list_name: The name of the attr to scan the dependencies from.
        provider: The concrete provider to ensure exists.
        what_provides: The name of the build rule that offers the required
            provider.
    """
    for dep in getattr(ctx.attr, list_name):
        if provider not in dep:
            fail(
                "{} in `{}` is not a {}".format(
                    dep.label,
                    list_name,
                    what_provides,
                ),
            )

def actionless_flag_set(flag_set_to_copy):
    """Copies a flag_set, stripping `actions`.

    Args:
        flag_set_to_copy: The base flag_set to copy.
    Returns:
        flag_set with empty `actions` list.
    """
    return flag_set(
        with_features = flag_set_to_copy.with_features,
        flag_groups = flag_set_to_copy.flag_groups,
    )
