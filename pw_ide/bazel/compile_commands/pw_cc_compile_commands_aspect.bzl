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
load("@rules_cc//cc:action_names.bzl", "ACTION_NAMES")
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain", "use_cc_toolchain")
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

_C_COMPILE_EXTS = [
    "c",
]
_CPP_COMPILE_EXTS = [
    "cc",
    "cpp",
]
_ALL_COMPILE_EXTS = _C_COMPILE_EXTS + _CPP_COMPILE_EXTS

# This value is somewhat arbitrary. It's roughly associated with the maximum
# number of direct dependencies any one target may have.
_MAX_STACK_ITERATIONS = 99999

def _get_one_compile_command(ctx, src, action):
    """Extracts compile commands associated with the source.

    Args:
        ctx: Rule context.
        src: The specific source file to extract a compile command for.
        action: The list of actions that may contain a compiler invocation
            for the requested source file.

    Returns:
        A single compile commands struct, or None.
    """
    if not src.extension in _ALL_COMPILE_EXTS:
        return None

    cc_toolchain = find_cc_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
    )
    action_name = (
        ACTION_NAMES.c_compile if src.extension in _C_COMPILE_EXTS else ACTION_NAMES.cpp_compile
    )
    tool = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = action_name,
    )
    if not action.argv or action.argv[0] != tool:
        return None

    return struct(
        directory = "__WORKSPACE_ROOT__",
        file = src.path,
        arguments = action.argv,
    )

def _get_one_header_compile_command(ctx, target, hdr):
    """Collects C/C++ compile commands for the provided target.

    This is slightly more fuzzy than the source file handling since C/C++
    headers inherently have no canonical argument representation. This rule
    exposes headers in their C++ form as any dependency of the rule will
    see them.

    Args:
        ctx: Rule context.
        target: The target to extract compile commands from.
        hdr: The header to generate a compile command for

    Returns:
        A single compile commands struct, or None.
    """
    cc_toolchain = find_cc_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
    )
    compilation_context = target[CcInfo].compilation_context
    compiler_exec = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = ACTION_NAMES.cpp_compile,
    )
    compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        # user_compile_flags - Omitted since these do not propagate.
        # source_file - Omitted because passing hdr causes a crash.
        # output_file - Omitted since this is a header.
        quote_include_directories = compilation_context.quote_includes,
        include_directories = compilation_context.includes,
        system_include_directories = compilation_context.system_includes,
        preprocessor_defines = compilation_context.defines,
        framework_include_directories = compilation_context.framework_includes,
    )

    # Assume all headers will be evaluated from C++.
    base_argv = list(cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = ACTION_NAMES.cpp_compile,
        variables = compile_variables,
    ))
    full_argv = [compiler_exec] + base_argv

    return struct(
        directory = "__WORKSPACE_ROOT__",
        file = hdr.path,
        arguments = full_argv,
    )

def _get_cpp_compile_commands(ctx, target):
    """Collects C/C++ compile commands for the provided target.

    Args:
        ctx: Rule context.
        target: The target to extract compile commands from.

    Returns:
        List of compile commands `struct` objects.
    """
    if CcInfo not in target:
        return []

    commands = []
    for action in target.actions:
        for src in action.inputs.to_list():
            result = _get_one_compile_command(ctx, src, action)
            if result != None:
                commands.append(result)

    cc_info = target[CcInfo]
    for hdr in cc_info.compilation_context.direct_headers:
        # TODO: https://pwbug.dev/438812970 - Dedupe/remap _virtual_includes.
        result = _get_one_header_compile_command(ctx, target, hdr)
        if result != None:
            commands.append(result)

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

    # Create a JSON fragment file for this specific target, including the
    # platform name to make it unique. We also add a unique suffix
    # to distinguish these fragments from files generated by other tools.
    platform_fragment = ctx.bin_dir.path.split("/")[1]

    dep_fragments = _collect_fragments(
        ctx.rule,
        target.label,
        CompileCommandsFragmentInfo,
        lambda command_fragment_info: command_fragment_info.fragments,
    )

    commands = _get_cpp_compile_commands(
        ctx,
        target,
    )
    if not commands:
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
            OutputGroupInfo(pw_cc_compile_commands_fragments = transitive_fragments),
        ]

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
        OutputGroupInfo(pw_cc_compile_commands_fragments = compile_command_fragments),
    ]

pw_cc_compile_commands_aspect = aspect(
    implementation = _compile_commands_aspect_impl,
    attr_aspects = ["*"],
    fragments = ["cpp"],
    attrs = {
        "_strict_errors": attr.label(default = "//pw_ide/bazel/compile_commands:strict_errors"),
    },
    toolchains = use_cc_toolchain(),
)
