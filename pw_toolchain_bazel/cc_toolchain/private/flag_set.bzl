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
"""Implementation of pw_cc_flag_set and pw_cc_flag_group."""

load(
    "@rules_cc//cc:cc_toolchain_config_lib.bzl",
    "flag_group",
)
load(
    ":providers.bzl",
    "PwActionNameSetInfo",
    "PwFeatureConstraintInfo",
    "PwFlagGroupInfo",
    "PwFlagSetInfo",
)

def _pw_cc_flag_group_impl(ctx):
    """Implementation for pw_cc_flag_group."""

    # If these are empty strings, they are handled differently than if they
    # are None. Rather than an explicit error or breakage, there's just silent
    # behavioral differences. Ideally, these attributes default to `None`, but
    # that is not supported with string types. Since these have no practical
    # meaning if they are empty strings, just remap empty strings to `None`.
    #
    # A minimal reproducer of this behavior with some useful analysis is
    # provided here:
    #
    #     https://github.com/armandomontanez/bazel_reproducers/tree/main/flag_group_with_empty_strings
    iterate_over = ctx.attr.iterate_over if ctx.attr.iterate_over else None
    expand_if = ctx.attr.expand_if_available if ctx.attr.expand_if_available else None
    expand_if_not = ctx.attr.expand_if_not_available if ctx.attr.expand_if_not_available else None
    return flag_group(
        flags = ctx.attr.flags,
        iterate_over = iterate_over,
        expand_if_available = expand_if,
        expand_if_not_available = expand_if_not,
    )

pw_cc_flag_group = rule(
    implementation = _pw_cc_flag_group_impl,
    attrs = {
        "flags": attr.string_list(
            doc = """List of flags provided by this rule.

For extremely complex expressions of flags that require nested flag groups with
multiple layers of expansion, prefer creating a custom rule in Starlark that
provides `PwFlagGroupInfo` or `PwFlagSetInfo`.
""",
        ),
        "iterate_over": attr.string(
            doc = """Expands `flags` for items in the named list.

Toolchain actions have various variables accessible as names that can be used
to guide flag expansions. For variables that are lists, `iterate_over` must be
used to expand the list into a series of flags.

Note that `iterate_over` is the string name of a build variable, and not an
actual list. Valid options are listed at:

    https://bazel.build/docs/cc-toolchain-config-reference#cctoolchainconfiginfo-build-variables

Note that the flag expansion stamps out the entire list of flags in `flags`
once for each item in the list.

Example:

    # Expands each path in `system_include_paths` to a series of `-isystem`
    # includes.
    #
    # Example input:
    #     system_include_paths = ["/usr/local/include", "/usr/include"]
    #
    # Expected result:
    #     "-isystem /usr/local/include -isystem /usr/include"
    pw_cc_flag_group(
        name = "system_include_paths",
        flags = ["-isystem", "%{system_include_paths}"],
        iterate_over = "system_include_paths",
    )
""",
        ),
        "expand_if_available": attr.string(
            doc = "Expands the expression in `flags` if the specified build variable is set.",
        ),
        "expand_if_not_available": attr.string(
            doc = "Expands the expression in `flags` if the specified build variable is NOT set.",
        ),
    },
    provides = [PwFlagGroupInfo],
    doc = """Declares an (optionally parametric) ordered set of flags.

`pw_cc_flag_group` rules are expected to be consumed exclusively by
`pw_cc_flag_set` rules. Though simple lists of flags can be expressed by
populating `flags` on a `pw_cc_flag_set`, `pw_cc_flag_group` provides additional
power in the following two ways:

    1. Iteration and conditional expansion. Using `iterate_over`,
       `expand_if_available`, and `expand_if_not_available`, more complex flag
       expressions can be made. This is critical for implementing things like
       the `libraries_to_link` feature, where library names are transformed
       into flags that end up in the final link invocation.

       Note: `expand_if_equal`, `expand_if_true`, and `expand_if_false` are not
       yet supported.

    2. Flags are tool-independent. A `pw_cc_flag_group` expresses ordered flags
       that may be reused across various `pw_cc_flag_set` rules. This is useful
       for cases where multiple `pw_cc_flag_set` rules must be created to
       implement a feature for which flags are slightly different depending on
       the action (e.g. compile vs link). Common flags can be expressed in a
       shared `pw_cc_flag_group`, and the differences can be relegated to
       separate `pw_cc_flag_group` instances.

Examples:

    pw_cc_flag_group(
        name = "user_compile_flag_expansion",
        flags = ["%{user_compile_flags}"],
        iterate_over = "user_compile_flags",
        expand_if_available = "user_compile_flags",
    )

    # This flag_group might be referenced from various FDO-related
    # `pw_cc_flag_set` rules. More importantly, the flag sets pulling this in
    # may apply to different sets of actions.
    pw_cc_flag_group(
        name = "fdo_profile_correction",
        flags = ["-fprofile-correction"],
        expand_if_available = "fdo_profile_path",
    )
""",
)

def _pw_cc_flag_set_impl(ctx):
    """Implementation for pw_cc_flag_set."""
    if ctx.attr.flags and ctx.attr.flag_groups:
        fail("{} specifies both `flag_groups` and `flags`, but only one can be specified. Consider splitting into two `pw_cc_flag_set` rules to make the intended order clearer.".format(ctx.label))
    flag_groups = []
    if ctx.attr.flags:
        flag_groups.append(flag_group(flags = ctx.attr.flags))
    elif ctx.attr.flag_groups:
        for dep in ctx.attr.flag_groups:
            if not dep[PwFlagGroupInfo]:
                fail("{} in `flag_groups` of {} does not provide PwFlagGroupInfo".format(dep.label, ctx.label))

        flag_groups = [dep[PwFlagGroupInfo] for dep in ctx.attr.flag_groups]

    actions = depset(transitive = [
        action[PwActionNameSetInfo].actions
        for action in ctx.attr.actions
    ]).to_list()
    if not actions:
        fail("Each pw_cc_flag_set must specify at least one action")

    requires = [fc[PwFeatureConstraintInfo] for fc in ctx.attr.requires_any_of]
    return [
        PwFlagSetInfo(
            label = ctx.label,
            actions = tuple(actions),
            requires_any_of = tuple(requires),
            flag_groups = tuple(flag_groups),
            env = ctx.attr.env,
            env_expand_if_available = ctx.attr.env_expand_if_available,
        ),
    ]

pw_cc_flag_set = rule(
    implementation = _pw_cc_flag_set_impl,
    attrs = {
        "actions": attr.label_list(
            providers = [PwActionNameSetInfo],
            mandatory = True,
            doc = """A list of action names that this flag set applies to.

See @pw_toolchain//actions:all for valid options.
""",
        ),
        "flag_groups": attr.label_list(
            doc = """Labels pointing to `pw_cc_flag_group` rules.

This is intended to be compatible with any other rules that provide
`PwFlagGroupInfo`. These are evaluated in order, with earlier flag groups
appearing earlier in the invocation of the underlying tool.

Note: `flag_groups` and `flags` are mutually exclusive.
""",
        ),
        "env": attr.string_dict(
            doc = "Environment variables to be added to these actions",
        ),
        "env_expand_if_available": attr.string(
            doc = "A build variable that needs to be available in order to expand the env entries",
        ),
        "flags": attr.string_list(
            doc = """Flags that should be applied to the specified actions.

These are evaluated in order, with earlier flags appearing earlier in the
invocation of the underlying tool. If you need expansion logic, prefer
enumerating flags in a `pw_cc_flag_group` or create a custom rule that provides
`PwFlagGroupInfo`.

Note: `flags` and `flag_groups` are mutually exclusive.
""",
        ),
        "requires_any_of": attr.label_list(
            providers = [PwFeatureConstraintInfo],
            doc = """This will be enabled when any of the constraints are met.

If omitted, this flag set will be enabled unconditionally.
""",
        ),
    },
    provides = [PwFlagSetInfo],
    doc = """Declares an ordered set of flags bound to a set of actions.

Flag sets can be attached to a `pw_cc_toolchain` via `flag_sets`.

Examples:

    pw_cc_flag_set(
        name = "warnings_as_errors",
        flags = ["-Werror"],
    )

    pw_cc_flag_set(
        name = "layering_check",
        flag_groups = [
            ":strict_module_headers",
            ":dependent_module_map_files",
        ],
    )

Note: In the vast majority of cases, alphabetical sorting is not desirable for
the `flags` and `flag_groups` attributes. Buildifier shouldn't ever try to sort
these, but in the off chance it starts to these members should be listed as
exceptions in the `SortableDenylist`.
""",
)
