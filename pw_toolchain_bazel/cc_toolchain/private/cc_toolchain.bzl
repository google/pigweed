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
"""Implementation of the pw_cc_toolchain rule."""

load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "action_config",
    "feature",
    "flag_group",
    "flag_set",
    "variable_with_value",
)
load(
    "//cc_toolchain/private:action_config_files.bzl",
    "pw_cc_action_config_file_collector",
)
load("//features:builtin_features.bzl", "BUILTIN_FEATURES")
load(
    ":providers.bzl",
    "PwActionConfigInfo",
    "PwActionConfigListInfo",
    "PwFeatureInfo",
    "PwFeatureSetInfo",
    "PwFlagSetInfo",
)
load(
    ":utils.bzl",
    "ALL_FILE_GROUPS",
    "actionless_flag_set",
    "to_untyped_config",
    "to_untyped_flag_set",
)

# These attributes of pw_cc_toolchain are deprecated.
PW_CC_TOOLCHAIN_DEPRECATED_TOOL_ATTRS = {
    "ar": "Path to the tool to use for `ar` (static link) actions",
    "cpp": "Path to the tool to use for C++ compile actions",
    "gcc": "Path to the tool to use for C compile actions",
    "gcov": "Path to the tool to use for generating code coverage data",
    "ld": "Path to the tool to use for link actions",
    "strip": "Path to the tool to use for strip actions",
    "objcopy": "Path to the tool to use for objcopy actions",
    "objdump": "Path to the tool to use for objdump actions",
}

PW_CC_TOOLCHAIN_CONFIG_ATTRS = {
    "action_configs": "List of `pw_cc_action_config` labels that bind tools to the appropriate actions",
    "action_config_flag_sets": "List of `pw_cc_flag_set`s to apply to their respective action configs",
    "toolchain_features": "List of `pw_cc_feature`s that this toolchain supports",

    # Attributes originally part of create_cc_toolchain_config_info.
    "toolchain_identifier": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "host_system_name": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "target_system_name": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "target_cpu": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "target_libc": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "compiler": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "abi_version": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "abi_libc_version": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "cc_target_os": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "builtin_sysroot": "See documentation for cc_common.create_cc_toolchain_config_info()",
    "cxx_builtin_include_directories": "See documentation for cc_common.create_cc_toolchain_config_info()",
}

PW_CC_TOOLCHAIN_SHARED_ATTRS = ["toolchain_identifier"]

PW_CC_TOOLCHAIN_BLOCKED_ATTRS = {
    "toolchain_config": "pw_cc_toolchain includes a generated toolchain config",
    "artifact_name_patterns": "pw_cc_toolchain does not yet support artifact name patterns",
    "features": "Use toolchain_features to add pw_cc_toolchain_feature deps to the toolchain",
    "tool_paths": "pw_cc_toolchain does not support tool_paths, use \"action_configs\" to set toolchain tools",
    "make_variables": "pw_cc_toolchain does not yet support make variables",
}

def _archiver_flags_feature(is_mac):
    """Returns our implementation of the legacy archiver_flags feature.

    We provide our own implementation of the archiver_flags.  The default
    implementation of this legacy feature at
    https://github.com/bazelbuild/bazel/blob/252d36384b8b630d77d21fac0d2c5608632aa393/src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java#L620-L660
    contains a bug that prevents it from working with llvm-libtool-darwin only
    fixed in
    https://github.com/bazelbuild/bazel/commit/ae7cfa59461b2c694226be689662d387e9c38427,
    which has not yet been released.

    However, we don't merely fix the bug. Part of the Pigweed build involves
    linking some empty libraries (with no object files). This leads to invoking
    the archiving tool with no input files. Such an invocation is considered a
    success by llvm-ar, but not by llvm-libtool-darwin. So for now, we use
    flags appropriate for llvm-ar here, even on MacOS.

    Args:
        is_mac: Does the toolchain this feature will be included in target MacOS?

    Returns:
        The archiver_flags feature.
    """

    # TODO: b/297413805 - Remove this implementation.
    return feature(
        name = "archiver_flags",
        flag_sets = [
            flag_set(
                actions = [
                    ACTION_NAMES.cpp_link_static_library,
                ],
                flag_groups = [
                    flag_group(
                        flags = _archiver_flags(is_mac),
                    ),
                    flag_group(
                        expand_if_available = "output_execpath",
                        flags = ["%{output_execpath}"],
                    ),
                ],
            ),
            flag_set(
                actions = [
                    ACTION_NAMES.cpp_link_static_library,
                ],
                flag_groups = [
                    flag_group(
                        expand_if_available = "libraries_to_link",
                        iterate_over = "libraries_to_link",
                        flag_groups = [
                            flag_group(
                                expand_if_equal = variable_with_value(
                                    name = "libraries_to_link.type",
                                    value = "object_file",
                                ),
                                flags = ["%{libraries_to_link.name}"],
                            ),
                            flag_group(
                                expand_if_equal = variable_with_value(
                                    name = "libraries_to_link.type",
                                    value = "object_file_group",
                                ),
                                flags = ["%{libraries_to_link.object_files}"],
                                iterate_over = "libraries_to_link.object_files",
                            ),
                        ],
                    ),
                ],
            ),
        ],
    )

def _extend_action_set_flags(action, flag_sets_by_action):
    extended_flags = flag_sets_by_action.get(action.action_name, default = [])
    for x in extended_flags:
        for y in action.flag_sets:
            if x == y:
                # TODO: b/311679764 - Propagate labels so we can raise the label
                # as part of the warning.
                fail("Flag set in `action_config_flag_sets` is already bound to the `{}` tool".format(action.action_name))
    return action_config(
        action_name = action.action_name,
        enabled = action.enabled,
        tools = action.tools,
        flag_sets = action.flag_sets + extended_flags,
        implies = action.implies,
    )

def _collect_action_configs(ctx, flag_sets_by_action):
    known_actions = {}
    action_configs = []
    for ac_dep in ctx.attr.action_configs:
        temp_actions = []
        if PwActionConfigInfo in ac_dep:
            temp_actions.append(ac_dep[PwActionConfigInfo])
        if PwActionConfigListInfo in ac_dep:
            temp_actions.extend([ac for ac in ac_dep[PwActionConfigListInfo].action_configs])
        if PwActionConfigListInfo not in ac_dep and PwActionConfigInfo not in ac_dep:
            fail(
                "{} in `action_configs` is not a `pw_cc_action_config`".format(
                    ac_dep.label,
                ),
            )
        for action in temp_actions:
            if action.action_name in known_actions:
                fail("In {} both {} and {} implement `{}`".format(
                    ctx.label,
                    ac_dep.label,
                    known_actions[action.action_name],
                    action.action_name,
                ))

            # Track which labels implement each action name for better error
            # reporting.
            known_actions[action.action_name] = ac_dep.label
            action_configs.append(_extend_action_set_flags(action, flag_sets_by_action))
    return action_configs

def _archiver_flags(is_mac):
    """Returns flags for llvm-ar."""
    if is_mac:
        return ["--format=darwin", "rcs"]
    else:
        return ["rcsD"]

def _create_action_flag_set_map(flag_sets):
    """Creates a mapping of action names to flag sets.

    Args:
        flag_sets: the flag sets to expand.

    Returns:
        Dictionary mapping action names to lists of PwFlagSetInfo providers.
    """
    flag_sets_by_action = {}
    for fs in flag_sets:
        handled_actions = {}
        for action in fs.actions:
            if action not in flag_sets_by_action:
                flag_sets_by_action[action] = []

            # Dedupe action set list.
            if action not in handled_actions:
                handled_actions[action] = True
                flag_sets_by_action[action].append(actionless_flag_set(fs))
    return flag_sets_by_action

def _pw_cc_toolchain_config_impl(ctx):
    """Rule that provides a CcToolchainConfigInfo.

    Args:
        ctx: The context of the current build rule.

    Returns:
        CcToolchainConfigInfo
    """
    flag_sets_by_action = _create_action_flag_set_map([
        to_untyped_flag_set(dep[PwFlagSetInfo], known = {})
        for dep in ctx.attr.action_config_flag_sets
    ])
    all_actions = _collect_action_configs(ctx, flag_sets_by_action)
    builtin_include_dirs = ctx.attr.cxx_builtin_include_directories if ctx.attr.cxx_builtin_include_directories else []
    sysroot_dir = ctx.attr.builtin_sysroot if ctx.attr.builtin_sysroot else None

    feature_set = PwFeatureSetInfo(features = depset(
        [ft[PwFeatureInfo] for ft in ctx.attr._builtin_features],
        transitive = [
            feature_set[PwFeatureSetInfo].features
            for feature_set in ctx.attr.toolchain_features
        ],
    ))
    out = to_untyped_config(feature_set)

    # TODO: b/297413805 - This could be externalized.
    out.features.append(_archiver_flags_feature(ctx.attr.target_libc == "macosx"))

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        action_configs = all_actions,
        features = out.features,
        cxx_builtin_include_directories = builtin_include_dirs,
        toolchain_identifier = ctx.attr.toolchain_identifier,
        host_system_name = ctx.attr.host_system_name,
        target_system_name = ctx.attr.target_system_name,
        target_cpu = ctx.attr.target_cpu,
        target_libc = ctx.attr.target_libc,
        compiler = ctx.attr.compiler,
        abi_version = ctx.attr.abi_version,
        abi_libc_version = ctx.attr.abi_libc_version,
        builtin_sysroot = sysroot_dir,
        cc_target_os = ctx.attr.cc_target_os,
    )

pw_cc_toolchain_config = rule(
    implementation = _pw_cc_toolchain_config_impl,
    attrs = {
        # Attributes new to this rule.
        "action_configs": attr.label_list(),
        "action_config_flag_sets": attr.label_list(providers = [PwFlagSetInfo]),
        "toolchain_features": attr.label_list(providers = [PwFeatureSetInfo]),

        # Attributes from create_cc_toolchain_config_info.
        "toolchain_identifier": attr.string(),
        "host_system_name": attr.string(),
        "target_system_name": attr.string(),
        "target_cpu": attr.string(),
        "target_libc": attr.string(),
        "compiler": attr.string(),
        "abi_version": attr.string(),
        "abi_libc_version": attr.string(),
        "cc_target_os": attr.string(),
        "builtin_sysroot": attr.string(),
        "cxx_builtin_include_directories": attr.string_list(),
        "_builtin_features": attr.label_list(default = BUILTIN_FEATURES),
    },
    provides = [CcToolchainConfigInfo],
)

def _check_args(rule_label, kwargs):
    """Checks that args provided to pw_cc_toolchain are valid.

    Args:
        rule_label: The label of the pw_cc_toolchain rule.
        kwargs: All attributes supported by pw_cc_toolchain.

    Returns:
        None
    """
    for attr_name, msg in PW_CC_TOOLCHAIN_BLOCKED_ATTRS.items():
        if attr_name in kwargs:
            fail(
                "Toolchain {} has an invalid attribute \"{}\": {}".format(
                    rule_label,
                    attr_name,
                    msg,
                ),
            )

def _split_args(kwargs, filter_dict):
    """Splits kwargs into two dictionaries guided by a filter.

    All items in the kwargs dictionary whose keys are present in the filter
    dictionary are returned as a new dictionary as the first item in the tuple.
    All remaining arguments are returned as a dictionary in the second item of
    the tuple.

    Args:
        kwargs: Dictionary of args to split.
        filter_dict: The dictionary used as the filter.

    Returns:
        Tuple[Dict, Dict]
    """
    filtered_args = {}
    remainder = {}

    for attr_name, val in kwargs.items():
        if attr_name in filter_dict:
            filtered_args[attr_name] = val
        else:
            remainder[attr_name] = val

    return filtered_args, remainder

def _generate_file_group(kwargs, attr_name, action_names):
    """Generates rules to collect files from pw_cc_action_config rules.

    All items in the kwargs dictionary whose keys are present in the filter
    dictionary are returned as a new dictionary as the first item in the tuple.
    All remaining arguments are returned as a dictionary in the second item of
    the tuple.

    Args:
        kwargs: Dictionary of all pw_cc_toolchain arguments.
        attr_name: The attr name of the file group to collect files for.
        action_names: The actions that apply to the `attr_name` group.

    Returns:
        Name of the generated filegroup rule.
    """
    file_group_name = "{}_{}".format(kwargs["name"], attr_name)
    pw_cc_action_config_file_collector(
        name = file_group_name,
        all_action_configs = kwargs["action_configs"],
        extra_files = kwargs[attr_name] if attr_name in kwargs else None,
        collect_files_from_actions = action_names,
        visibility = ["//visibility:private"],
    )
    return file_group_name

def _generate_misc_file_group(kwargs):
    """Generate the misc_files group.

    Some actions aren't enumerated in ALL_FILE_GROUPS because they don't
    necessarily have an associated *_files group. This group collects
    all the other files and enumerates them in a group so they still appear in
    all_files.

    Args:
        kwargs: Dictionary of all pw_cc_toolchain arguments.

    Returns:
        Name of the generated filegroup rule.
    """
    file_group_name = "{}_misc_files".format(kwargs["name"])

    all_known_actions = []
    for action_names in ALL_FILE_GROUPS.values():
        all_known_actions.extend(action_names)

    pw_cc_action_config_file_collector(
        name = file_group_name,
        all_action_configs = kwargs["action_configs"],
        collect_files_not_from_actions = all_known_actions,
        visibility = ["//visibility:private"],
    )
    return file_group_name

def pw_cc_toolchain(**kwargs):
    """A suite of cc_toolchain, pw_cc_toolchain_config, and *_files rules.

    Generated rules:
        {name}: A `cc_toolchain` for this toolchain.
        {name}_config: A `pw_cc_toolchain_config` for this toolchain.
        {name}_*_files: Generated rules that group together files for
            "all_files", "ar_files", "as_files", "compiler_files",
            "coverage_files", "dwp_files", "linker_files", "objcopy_files", and
            "strip_files" normally enumerated as part of the `cc_toolchain`
            rule.
        {name}_misc_files: Generated rule that groups together files for action
            configs not associated with any other *_files group.

    Args:
        **kwargs: All attributes supported by either cc_toolchain or pw_cc_toolchain_config.
    """

    _check_args(native.package_relative_label(kwargs["name"]), kwargs)

    # Generate *_files groups.
    # `all_files` is skipped here because it is handled differently below.
    for group_name, action_names in ALL_FILE_GROUPS.items():
        kwargs[group_name] = _generate_file_group(kwargs, group_name, action_names)

    # The `all_files` group must be a superset of all the smaller file groups.
    all_files_name = "{}_all_files".format(kwargs["name"])
    all_file_inputs = [":{}".format(kwargs[file_group]) for file_group in ALL_FILE_GROUPS.keys()]

    all_file_inputs.append(":{}".format(_generate_misc_file_group(kwargs)))

    if "all_files" in kwargs:
        all_file_inputs.append(kwargs["all_files"])
    native.filegroup(
        name = all_files_name,
        srcs = all_file_inputs,
        visibility = ["//visibility:private"],
    )
    kwargs["all_files"] = ":{}".format(all_files_name)

    # Split args between `pw_cc_toolchain_config` and `native.cc_toolchain`.
    cc_toolchain_config_args, cc_toolchain_args = _split_args(kwargs, PW_CC_TOOLCHAIN_CONFIG_ATTRS | PW_CC_TOOLCHAIN_DEPRECATED_TOOL_ATTRS)

    # Bind pw_cc_toolchain_config and the cc_toolchain.
    config_name = "{}_config".format(cc_toolchain_args["name"])
    cc_toolchain_config_args["name"] = config_name
    cc_toolchain_args["toolchain_config"] = ":{}".format(config_name)

    # TODO: b/321268080 - Remove after transition of this option is complete.
    cc_toolchain_args["exec_transition_for_inputs"] = False

    # Copy over arguments that should be shared by both rules.
    for arg_name in PW_CC_TOOLCHAIN_SHARED_ATTRS:
        if arg_name in cc_toolchain_config_args:
            cc_toolchain_args[arg_name] = cc_toolchain_config_args[arg_name]

    pw_cc_toolchain_config(**cc_toolchain_config_args)
    native.cc_toolchain(**cc_toolchain_args)
