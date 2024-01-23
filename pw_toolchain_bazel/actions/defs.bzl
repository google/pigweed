# Copyright 2024 The Pigweed Authors
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
"""Rules to turn action names into bazel labels."""

load(":providers.bzl", "ActionNameInfo", "ActionNameSetInfo")

visibility("//cc_toolchain")

def _pw_cc_action_name_impl(ctx):
    return [
        ActionNameInfo(name = ctx.attr.action_name),
        ActionNameSetInfo(actions = depset([ctx.attr.action_name])),
    ]

pw_cc_action_name = rule(
    implementation = _pw_cc_action_name_impl,
    attrs = {
        "action_name": attr.string(
            mandatory = True,
        ),
    },
    doc = """The name of a single type of action.

This rule represents a type-safe definition for an action name.
Listing this in a pw_cc_flag_set or pw_cc_action tells a toolchain which actions
apply to the attached flags or tools.

Example:

load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")

pw_cc_action_name(
  name = "cpp_compile",
  action_name =  = ACTION_NAMES.cpp_compile,
)

pw_cc_flag_set(
    name = "c++17",
    actions = [":cpp_compile"],
    flags = ["-std=c++17"],
)
""",
    provides = [ActionNameInfo, ActionNameSetInfo],
)

def _pw_cc_action_name_set_impl(ctx):
    return [ActionNameSetInfo(actions = depset(transitive = [
        attr[ActionNameSetInfo].actions
        for attr in ctx.attr.actions
    ]))]

pw_cc_action_name_set = rule(
    doc = """A set of action names.

This rule represents a group of one or more pw_cc_action_name rules.
This can be used in place of a pw_cc_action_name for rule attributes that
accept multiple action names.

Example:

pw_cc_action_name_set(
  name = "all_cpp_compiler_actions",
  actions = [":cpp_compile", ":cpp_header_parsing"],
)

pw_cc_flag_set(
    name = "c++17",
    actions = [":all_cpp_compiler_actions"],
    flags = ["-std=c++17"],
)
""",
    implementation = _pw_cc_action_name_set_impl,
    attrs = {
        "actions": attr.label_list(
            providers = [ActionNameSetInfo],
            mandatory = True,
            doc = "A list of pw_cc_action_name or pw_cc_action_name_set to be combined.",
        ),
    },
    provides = [ActionNameSetInfo],
)
