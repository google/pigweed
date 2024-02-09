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
    "@rules_cc//cc:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "variable_with_value",
)
load("//features:builtin_features.bzl", "BUILTIN_FEATURES")
load(
    ":providers.bzl",
    "PwActionConfigSetInfo",
    "PwActionNameSetInfo",
    "PwExtraActionFilesSetInfo",
    "PwFeatureInfo",
    "PwFeatureSetInfo",
    "PwFlagSetInfo",
    "PwToolchainConfigInfo",
)
load(
    ":utils.bzl",
    "ALL_FILE_GROUPS",
    "to_untyped_config",
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
    "unconditional_flag_sets": "List of `pw_cc_flag_set`s to apply to their respective action configs",
    "toolchain_features": "List of `pw_cc_feature`s that this toolchain supports",
    "extra_action_files": "Files that are required to run specific actions.",

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

def _archiver_flags(is_mac):
    """Returns flags for llvm-ar."""
    if is_mac:
        return ["--format=darwin", "rcs"]
    else:
        return ["rcsD"]

def _pw_cc_toolchain_config_impl(ctx):
    """Rule that provides a CcToolchainConfigInfo.

    Args:
        ctx: The context of the current build rule.

    Returns:
        CcToolchainConfigInfo
    """
    builtin_include_dirs = ctx.attr.cxx_builtin_include_directories if ctx.attr.cxx_builtin_include_directories else []
    sysroot_dir = ctx.attr.builtin_sysroot if ctx.attr.builtin_sysroot else None

    feature_set = PwFeatureSetInfo(features = depset(
        [ft[PwFeatureInfo] for ft in ctx.attr._builtin_features],
        transitive = [
            feature_set[PwFeatureSetInfo].features
            for feature_set in ctx.attr.toolchain_features
        ],
    ))
    action_config_set = PwActionConfigSetInfo(
        label = ctx.label,
        action_configs = depset(transitive = [
            acs[PwActionConfigSetInfo].action_configs
            for acs in ctx.attr.action_configs
        ]),
    )
    extra_action_files = PwExtraActionFilesSetInfo(srcs = depset(transitive = [
        ffa[PwExtraActionFilesSetInfo].srcs
        for ffa in ctx.attr.extra_action_files
    ]))
    flag_sets = [fs[PwFlagSetInfo] for fs in ctx.attr.unconditional_flag_sets]
    out = to_untyped_config(feature_set, action_config_set, flag_sets, extra_action_files)

    # TODO: b/297413805 - This could be externalized.
    out.features.append(_archiver_flags_feature(ctx.attr.target_libc == "macosx"))

    extra = []
    if ctx.attr.extra_files:
        extra.append(ctx.attr.extra_files[DefaultInfo].files)
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            action_configs = out.action_configs,
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
        ),
        PwToolchainConfigInfo(action_to_files = out.action_to_files),
        DefaultInfo(files = depset(transitive = extra + out.action_to_files.values())),
    ]

pw_cc_toolchain_config = rule(
    implementation = _pw_cc_toolchain_config_impl,
    attrs = {
        # Attributes new to this rule.
        "action_configs": attr.label_list(providers = [PwActionConfigSetInfo]),
        "unconditional_flag_sets": attr.label_list(providers = [PwFlagSetInfo]),
        "toolchain_features": attr.label_list(providers = [PwFeatureSetInfo]),
        "extra_action_files": attr.label_list(providers = [PwExtraActionFilesSetInfo]),
        "extra_files": attr.label(),

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
    provides = [CcToolchainConfigInfo, PwToolchainConfigInfo],
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

def _cc_file_collector_impl(ctx):
    actions = depset(transitive = [
        names[PwActionNameSetInfo].actions
        for names in ctx.attr.actions
    ]).to_list()
    action_to_files = ctx.attr.config[PwToolchainConfigInfo].action_to_files

    extra = []
    if ctx.attr.extra_files:
        extra.append(ctx.attr.extra_files[DefaultInfo].files)
    return [DefaultInfo(files = depset(transitive = [
        action_to_files[action]
        for action in actions
    ] + extra))]

_cc_file_collector = rule(
    implementation = _cc_file_collector_impl,
    attrs = {
        "config": attr.label(providers = [PwToolchainConfigInfo], mandatory = True),
        "actions": attr.label_list(providers = [PwActionNameSetInfo], mandatory = True),
        "extra_files": attr.label(),
    },
)

def pw_cc_toolchain(name, action_config_flag_sets = None, **kwargs):
    """A suite of cc_toolchain, pw_cc_toolchain_config, and *_files rules.

    Generated rules:
        {name}: A `cc_toolchain` for this toolchain.
        _{name}_config: A `pw_cc_toolchain_config` for this toolchain.
        _{name}_*_files: Generated rules that group together files for
            "ar_files", "as_files", "compiler_files", "coverage_files",
            "dwp_files", "linker_files", "objcopy_files", and "strip_files"
            normally enumerated as part of the `cc_toolchain` rule.

    Args:
        name: str: The name of the label for the toolchain.
        action_config_flag_sets: Deprecated. Do not use.
        **kwargs: All attributes supported by either cc_toolchain or pw_cc_toolchain_config.
    """

    # TODO(b/322872628): Remove this once it's no longer in use.
    if action_config_flag_sets != None:
        kwargs["unconditional_flag_sets"] = action_config_flag_sets

    _check_args(native.package_relative_label(name), kwargs)

    # Split args between `pw_cc_toolchain_config` and `native.cc_toolchain`.
    cc_toolchain_config_args, cc_toolchain_args = _split_args(kwargs, PW_CC_TOOLCHAIN_CONFIG_ATTRS | PW_CC_TOOLCHAIN_DEPRECATED_TOOL_ATTRS)

    # Bind pw_cc_toolchain_config and the cc_toolchain.
    config_name = "_{}_config".format(name)
    pw_cc_toolchain_config(
        name = config_name,
        extra_files = cc_toolchain_args.pop("all_files", None),
        visibility = ["//visibility:private"],
        **cc_toolchain_config_args
    )

    all_files_srcs = [config_name]
    for group, actions in ALL_FILE_GROUPS.items():
        group_name = "_{}_{}".format(name, group)
        _cc_file_collector(
            name = group_name,
            config = config_name,
            actions = actions,
            extra_files = cc_toolchain_args.pop(group, None),
            visibility = ["//visibility:private"],
        )
        cc_toolchain_args[group] = group_name
        all_files_srcs.append(group_name)

    # Copy over arguments that should be shared by both rules.
    for arg_name in PW_CC_TOOLCHAIN_SHARED_ATTRS:
        if arg_name in cc_toolchain_config_args:
            cc_toolchain_args[arg_name] = cc_toolchain_config_args[arg_name]

    all_files_name = "_{}_all_files".format(name)
    native.filegroup(
        name = all_files_name,
        srcs = all_files_srcs,
    )

    native.cc_toolchain(
        name = name,
        toolchain_config = config_name,
        # TODO: b/321268080 - Remove after transition of this option is complete.
        exec_transition_for_inputs = False,
        # TODO(b/323448214): Replace with `all_files = config_name`.
        # This is currently required in case the user passes in compiler_files,
        # for example (since it won't propagate to the config.
        all_files = all_files_name,
        **cc_toolchain_args
    )
