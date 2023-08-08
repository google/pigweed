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

load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "//cc_toolchain/private:providers.bzl",
    "ToolchainFeatureInfo",
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
    "asmopts": ALL_ASM_ACTIONS,
    "copts": ALL_C_COMPILER_ACTIONS + ALL_CPP_COMPILER_ACTIONS,
    "conlyopts": ALL_C_COMPILER_ACTIONS,
    "cxxopts": ALL_CPP_COMPILER_ACTIONS,
    "linkopts": ALL_LINK_ACTIONS,
}

def check_deps(ctx):
    for dep in ctx.attr.feature_deps:
        if ToolchainFeatureInfo not in dep:
            fail(
                "{} listed as a dependency of {}, but it's not a pw_cc_toolchain_feature".format(
                    dep.label,
                    ctx.label,
                ),
            )
