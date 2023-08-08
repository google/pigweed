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
"""Implementation of the pw_cc_toolchain_feature rule."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
)
load(
    "//cc_toolchain/private:providers.bzl",
    "ToolchainFeatureInfo",
)
load("//cc_toolchain/private:utils.bzl", "ACTION_MAP")

TOOLCHAIN_FEATURE_INFO_ATTRS = {
    "asmopts": "List[str]: Flags to pass to assembler actions",
    "copts": "List[str]: Flags to pass to all C/C++ compile actions",
    "conlyopts": "List[str]: Flags to pass to C compile actions",
    "cxxopts": "List[str]: Flags to pass to C++ compile actions",
    "linkopts": "List[str]: Flags to pass to C compile actions",
    "linker_files": "List[File]: Files to link",
    "cxx_builtin_include_directories": "List[str]: Paths to C++ standard library include directories",
}

def _dict_to_str(dict_to_stringify):
    """Converts a dictionary to a multi-line string.

    Args:
        dict_to_stringify (Dict[str, str]): Dictionary to stringify.

    Returns:
        str: Multi-line string representing the dictionary, or {empty}.
    """
    result = []
    for key in dict_to_stringify.keys():
        result.append("    {}: {}".format(key, dict_to_stringify[key]))
    if not result:
        return "{empty}"
    return "\n".join(["{"] + result + ["}"])

def _feature_flag_set(actions, flags):
    """Transforms a list of flags and actions into a flag_set.

    Args:
        actions (List[str]): Actions that the provided flags will be applied to.
        flags (List[str]): List of flags to apply to the specified actions.

    Returns:
        flag_set: A flag_set binding the provided flags to the specified
          actions.
    """
    return flag_set(
        actions = actions,
        flag_groups = ([
            flag_group(
                flags = flags,
            ),
        ]),
    )

def _check_args(ctx, **kwargs):
    """Checks that args provided to build_toolchain_feature_info are valid.

    Args:
        ctx: The context of the current build rule.
        **kwargs: All attributes supported by pw_cc_toolchain_feature.

    Returns:
        None
    """
    for key in kwargs.keys():
        if key not in TOOLCHAIN_FEATURE_INFO_ATTRS:
            fail(
                "Unknown attribute \"{}\" used by {}. Valid attributes are:\n{}".format(
                    key,
                    ctx.label,
                    _dict_to_str(TOOLCHAIN_FEATURE_INFO_ATTRS),
                ),
            )

def _build_flag_sets(**kwargs):
    """Transforms a dictionary of arguments into a list of flag_sets.

    Args:
        **kwargs: All attributes supported by pw_cc_toolchain_feature.

    Returns:
        List[flag_set]: A list of flag_sets that bind all provided flags to
          their appropriate actions.
    """
    all_flags = []
    for action in ACTION_MAP.keys():
        if kwargs[action]:
            all_flags.append(_feature_flag_set(ACTION_MAP[action], kwargs[action]))
    return all_flags

def _initialize_args(**kwargs):
    """Initializes build_toolchain_feature_info arguments.

    Args:
        **kwargs: All attributes supported by pw_cc_toolchain_feature.

    Returns:
        Dict[str, Any]: Dictionary containing arguments default initialized to
          be compatible with _build_flag_sets.
    """
    initialized_args = {}
    for action in ACTION_MAP.keys():
        if action in kwargs:
            initialized_args[action] = kwargs[action]
        else:
            initialized_args[action] = []

    if "linker_files" in kwargs:
        linker_files = kwargs["linker_files"]
        linker_flags = [file.path for file in linker_files]

        initialized_args["linkopts"] = initialized_args["linkopts"] + linker_flags
        initialized_args["linker_files"] = depset(linker_files)
    else:
        initialized_args["linker_files"] = depset()

    if "cxx_builtin_include_directories" in kwargs:
        initialized_args["cxx_builtin_include_directories"] = kwargs["cxx_builtin_include_directories"]
    else:
        initialized_args["cxx_builtin_include_directories"] = []
    return initialized_args

def build_toolchain_feature_info(ctx, **kwargs):
    """Builds a ToolchainFeatureInfo provider.

    Args:
        ctx: The context of the current build rule.
        **kwargs: All attributes supported by pw_cc_toolchain_feature.

    Returns:
        ToolchainFeatureInfo, DefaultInfo: All providers supported by
          pw_cc_toolchain_feature.
    """
    _check_args(ctx, **kwargs)

    initialized_args = _initialize_args(**kwargs)

    new_feature = feature(
        name = ctx.attr.name,
        enabled = True,
        flag_sets = _build_flag_sets(**initialized_args),
    )

    return [
        ToolchainFeatureInfo(
            feature = new_feature,
            cxx_builtin_include_directories = initialized_args["cxx_builtin_include_directories"],
        ),
        DefaultInfo(files = initialized_args["linker_files"]),
    ]

def _pw_cc_toolchain_feature_impl(ctx):
    """Rule that provides ToolchainFeatureInfo.

    Args:
        ctx: The context of the current build rule.

    Returns:
        ToolchainFeatureInfo, DefaultInfo
    """
    return build_toolchain_feature_info(
        ctx = ctx,
        asmopts = ctx.attr.asmopts,
        copts = ctx.attr.copts,
        conlyopts = ctx.attr.conlyopts,
        cxxopts = ctx.attr.cxxopts,
        linkopts = ctx.attr.linkopts,
        linker_files = ctx.files.linker_files,
        cxx_builtin_include_directories = ctx.attr.cxx_builtin_include_directories,
    )

pw_cc_toolchain_feature = rule(
    implementation = _pw_cc_toolchain_feature_impl,
    attrs = {
        "asmopts": attr.string_list(),
        "copts": attr.string_list(),
        "conlyopts": attr.string_list(),
        "cxxopts": attr.string_list(),
        "linkopts": attr.string_list(),
        "linker_files": attr.label_list(allow_files = True),
        "cxx_builtin_include_directories": attr.string_list(),
    },
    provides = [ToolchainFeatureInfo, DefaultInfo],
)
