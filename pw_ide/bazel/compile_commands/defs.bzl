# Copyright 2025 The Pigweed Authors
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
"""
A self-contained Bazel aspect to generate compile_commands.json fragments.

NOTE: This functionality may eventually belong in rules_cc. See b/437157251
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

CompileCommandsFragmentInfo = provider(
    "Provides the depset of compile command fragment files.",
    fields = {
        "fragments": "A depset of compile_commands.json fragment files.",
    },
)

# This is the set of attribute types that do not expose targets.
_NO_PROVIDER_ATTR_TYPES = set([
    "bool",
    "int",
    "float",
    "string",
    "Label",
    "License",
    "NoneType",
])

# This value is somewhat arbitrary. It's roughly associated with the maximum
# number of direct dependencies any one target may have.
_MAX_STACK_ITERATIONS = 999

def _get_compile_commands(ctx, target, cc_toolchain, feature_configuration, target_srcs):
    """Reconstructs the compile command for each source file in a target."""
    commands = []
    compilation_context = target[CcInfo].compilation_context

    compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        user_compile_flags = ctx.fragments.cpp.cxxopts + ctx.fragments.cpp.copts,
    )
    base_argv = list(cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = "c++-compile",
        variables = compile_variables,
    ))

    compiler_exec = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = "c++-compile",
    )

    includes_and_defines = []
    for p in compilation_context.quote_includes.to_list():
        includes_and_defines.extend(["-iquote", p])
    for p in compilation_context.includes.to_list():
        includes_and_defines.extend(["-I", p])
    for p in compilation_context.system_includes.to_list():
        includes_and_defines.extend(["-isystem", p])
    for d in compilation_context.defines.to_list():
        includes_and_defines.append("-D" + d)

    for src in target_srcs:
        full_argv = [compiler_exec] + base_argv + includes_and_defines
        full_argv.extend(["-c", src.path])
        full_argv.extend(["-o", src.path + ".o"])

        commands.append(struct(
            directory = "__WORKSPACE_ROOT__",
            file = src.path,
            arguments = full_argv,
        ))

    return commands

def _collect_fragments(ctx, label, requested_provider, depset_getter):
    """A Generic helper to build a depset from all a transitive dependencies.

    Args:
        ctx: Rule context.
        label: Label of the current target under evaluation, used for error
            reporting.
        requested_provider: The provider to collect a depset from.
        depset_getter: A callback that takes the requested provider and extracts
            the correct depset from the provider.

    Returns:
        A list of depsets.
    """

    # NOTE: This looks like it should be a recursive method, but recursion is
    # strictly prohibited in Bazel/starlark. This is implemented as a loop with
    # a work stack instead.
    transitive_providers = []
    stack = [
        getattr(ctx.attr, attr_name)
        for attr_name in dir(ctx.attr)
    ]
    for i in range(_MAX_STACK_ITERATIONS + 1):
        if i == _MAX_STACK_ITERATIONS:
            fail(
                "{} has too many direct dependencies.".format(label),
                "Reached maximum depth ({})".format(_MAX_STACK_ITERATIONS),
                "of direct dependency expansion.",
            )
        if not stack:
            break
        actual_attr = stack.pop()
        if type(actual_attr) in _NO_PROVIDER_ATTR_TYPES:
            continue

        if type(actual_attr) == "Target":
            if requested_provider not in actual_attr:
                continue
            transitive_providers.append(
                depset_getter(actual_attr[requested_provider]),
            )
            continue

        if type(actual_attr) == "list":
            stack.extend(actual_attr)
            continue

        if type(actual_attr) == "dict":
            stack.extend(actual_attr.keys())
            stack.extend(actual_attr.values())
            continue

        if ctx.attr._strict_errors[BuildSettingInfo].value:
            # Note to maintainers:
            #   * If the unhandled type is a data structure that may contain
            #     Target objects, handle it above.
            #   * Otherwise, add to _NO_PROVIDER_ATTR_TYPES.
            fail(
                "Unhandled type: {}".format(type(actual_attr)),
                "",
                "This means a new attribute has been introduced into Bazel.",
                "To work around this:",
                "   * Set {}=False".format(ctx.attr._strict_errors.label),
                "   * Report this failure to Pigweed, making sure to include ",
                "     the Bazel version you are using.",
                sep = "\n",
            )

    return transitive_providers

def _compile_commands_aspect_impl(target, ctx):
    """Generates a compile_commands.json fragment for a single target."""
    srcs = []
    if hasattr(ctx.rule.attr, "srcs"):
        for src in ctx.rule.attr.srcs:
            srcs.extend(src.files.to_list())
    if hasattr(ctx.rule.attr, "hdrs"):
        for hdr in ctx.rule.attr.hdrs:
            srcs.extend(hdr.files.to_list())

    compilable_srcs = [
        s
        for s in srcs
        if s.extension in ["c", "cc", "cpp", "h", "hh", "hpp"]
    ]

    dep_fragments = _collect_fragments(
        ctx.rule,
        target.label,
        CompileCommandsFragmentInfo,
        lambda command_fragment_info: command_fragment_info.fragments,
    )

    # Create a JSON fragment file for this specific target, including the
    # platform name to make it unique. We also add a unique suffix
    # to distinguish these fragments from files generated by other tools.
    platform_fragment = ctx.bin_dir.path.split("/")[1]

    if not compilable_srcs or CcInfo not in target:
        # No compiled sources, so just forward the transitive fragments.
        transitive_fragments = depset(
            transitive = dep_fragments,
        )
        return [
            # Always return the transitive fragments. This ensures that
            # transitive dependencies chain across targets that do not contain
            # compile commands.
            CompileCommandsFragmentInfo(fragments = transitive_fragments),
            # Always return this OutputGroupInfo, or transitive compile commands
            # will not be generated if the target itself doesn't emit compile
            # commands.
            OutputGroupInfo(compile_commands_fragments = transitive_fragments),
        ]

    cc_toolchain = ctx.toolchains["@bazel_tools//tools/cpp:toolchain_type"].cc
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
    )

    commands = _get_compile_commands(
        ctx,
        target,
        cc_toolchain,
        feature_configuration,
        compilable_srcs,
    )

    fragment_file = ctx.actions.declare_file(
        ctx.label.name + "." + platform_fragment + ".pw_aspect.compile_commands.json",
    )
    ctx.actions.write(
        output = fragment_file,
        content = json.encode(commands),
    )

    compile_command_fragments = depset(
        direct = [fragment_file],
        transitive = dep_fragments,
    )

    return [
        CompileCommandsFragmentInfo(fragments = compile_command_fragments),
        OutputGroupInfo(compile_commands_fragments = compile_command_fragments),
    ]

compile_commands_aspect = aspect(
    implementation = _compile_commands_aspect_impl,
    attr_aspects = ["*"],
    fragments = ["cpp"],
    attrs = {
        "_strict_errors": attr.label(default = "//pw_ide/bazel/compile_commands:strict_errors"),
    },
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)
