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
"""Implementation of pw_cc_action_config and pw_cc_tool."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "FlagSetInfo",
    "ToolInfo",
    "action_config",
    config_lib_tool = "tool",  # This is renamed to reduce name aliasing.
)
load(
    "//cc_toolchain/private:providers.bzl",
    "ActionConfigListInfo",
)
load(
    "//cc_toolchain/private:utils.bzl",
    "actionless_flag_set",
    "check_deps_provide",
)

def _pw_cc_tool_impl(ctx):
    """Implementation for pw_cc_tool."""

    # Remaps empty strings to `None` to match behavior of the default values.
    tool = ctx.executable.tool if ctx.executable.tool else None
    path = ctx.attr.path if ctx.attr.path else None
    return config_lib_tool(
        tool = tool,
        path = path,
        execution_requirements = ctx.attr.execution_requirements,
    )

pw_cc_tool = rule(
    implementation = _pw_cc_tool_impl,
    attrs = {
        "tool": attr.label(
            allow_single_file = True,
            executable = True,
            cfg = "exec",
            doc = """The underlying tool that this rule represents.

This attribute is a label rather than a simple file path. This means that the
file must be referenced relative to the BUILD file that exports it. For example:

    @llvm_toolchain//:bin/clang
    ^              ^  ^
    Where:

    * `@llvm_toolchain` is the repository.
    * `//` is the directory of the BUILD file that exports the file of interest.
    * `bin/clang` is the path of the actual binary relative to the BUILD file of
      interest.
""",
        ),
        "path": attr.string(
            doc = """An absolute path to a binary to use for this tool.

Relative paths are also supported, but they are relative to the
`pw_cc_toolchain` that uses this tool rather than relative to this `pw_cc_tool`
rule.

WARNING: This method of listing a tool is NOT recommended, and is provided as an
escape hatch for edge cases. Prefer using `tool` whenever possible.
""",
        ),
        "execution_requirements": attr.string_list(
            doc = "A list of strings that provide hints for execution environment compatibility (e.g. `requires-darwin`).",
        ),
    },
    provides = [ToolInfo],
    doc = """Declares a singular tool that can be bound to action configs.

`pw_cc_tool` rules are intended to be consumed exclusively by
`pw_cc_action_config` rules. These rules declare an underlying tool that can
be used to fulfill various actions. Many actions may reuse a shared tool.

Note: `with_features` is not yet supported.

Example:

        # A project-provided tool.
        pw_cc_tool(
            name = "clang_tool",
            tool = "@llvm_toolchain//:bin/clang",
        )

        # A tool expected to be preinstalled on a user's machine.
        pw_cc_tool(
            name = "clang_tool",
            path = "/usr/bin/clang",
        )
""",
)

def _generate_action_config(ctx, action_name):
    flag_sets = []
    for fs in ctx.attr.flag_sets:
        provided_fs = fs[FlagSetInfo]
        if action_name in provided_fs.actions:
            flag_sets.append(actionless_flag_set(provided_fs))
    return action_config(
        action_name = action_name,
        enabled = ctx.attr.enabled,
        tools = [tool[ToolInfo] for tool in ctx.attr.tools],
        flag_sets = flag_sets,
        implies = ctx.attr.implies,
    )

def _pw_cc_action_config_impl(ctx):
    """Implementation for pw_cc_tool."""
    if not ctx.attr.tools:
        fail("Action configs are not valid unless they specify at least one `pw_cc_tool` in `tools`")
    if not ctx.attr.action_names:
        fail("Action configs are not valid unless they specify at least one action name in `action_names`")

    check_deps_provide(ctx, "tools", ToolInfo, "pw_cc_tool")

    # Check that the listed flag sets apply to at least one action in this group
    # of action configs.
    check_deps_provide(ctx, "flag_sets", FlagSetInfo, "pw_cc_flag_set")
    for fs in ctx.attr.flag_sets:
        provided_fs = fs[FlagSetInfo]
        flag_set_applies = False
        for action in ctx.attr.action_names:
            if action in provided_fs.actions:
                flag_set_applies = True
        if not flag_set_applies:
            fail("{} listed as a flag set to apply to {}, but none of the actions match".format(
                fs.label,
                ctx.label,
            ))

    return ActionConfigListInfo(
        action_configs = [_generate_action_config(ctx, action) for action in ctx.attr.action_names],
    )

pw_cc_action_config = rule(
    implementation = _pw_cc_action_config_impl,
    attrs = {
        "action_names": attr.string_list(
            # inclusive-language: disable
            mandatory = True,
            doc = """A list of action names to apply this action to.

Valid choices are listed here:

    https://github.com/bazelbuild/bazel/blob/master/tools/build_defs/cc/action_names.bzl

It is possible for some needed action names to not be enumerated in this list,
so there is not rigid validation for these strings. Prefer using constants
rather than manually typing action names.
""",
            # inclusive-language: enable
        ),
        "enabled": attr.bool(
            default = True,
            doc = """Whether or not this action config is enabled by default.

Note: This defaults to `True` since it's assumed that most listed action configs
will be enabled and used by default. This is the opposite of Bazel's native
default.
""",
        ),
        "tools": attr.label_list(
            mandatory = True,
            doc = """The `pw_cc_tool` to use for the specified actions.

If multiple tools are specified, the first tool that has `with_features` that
satisfy the currently enabled feature set is used.
""",
        ),
        "flag_sets": attr.label_list(
            doc = """Labels that point to `pw_cc_flag_set`s that are
unconditionally bound to the specified actions.

Note: The flags in the `pw_cc_flag_set` are only bound to matching action names.
If an action is listed in this rule's `action_names`, but is NOT listed in the
`pw_cc_flag_set`'s `actions`, the flag will not be applied to that action.
""",
        ),
        "implies": attr.string_list(
            doc = """Names of features that should be automatically enabled when
this tool is used.

WARNING: If this action config implies an unknown feature, this action config
will silently be disabled. This behavior is native to Bazel itself, and there's
no way to detect this and emit an error instead. For this reason, be very
cautious when listing implied features!
""",
        ),
    },
    provides = [ActionConfigListInfo],
    doc = """Declares the configuration and selection of `pw_cc_tool` rules.

Action configs are bound to a toolchain through `action_configs`, and are the
driving mechanism for controlling toolchain tool invocation/behavior.

Action configs define three key things:

* Which tools to invoke for a given type of action.
* Tool features and compatibility.
* `pw_cc_flag_set`s that are unconditionally bound to a tool invocation.

Examples:

    pw_cc_action_config(
        name = "ar",
        action_names = ALL_AR_ACTIONS,
        implies = [
            "archiver_flags",
            "linker_param_file",
        ],
        tools = [":ar_tool"],
    )

    pw_cc_action_config(
        name = "clang",
        action_names = ALL_ASM_ACTIONS + ALL_C_COMPILER_ACTIONS,
        tools = [":clang_tool"],
    )
""",
)
