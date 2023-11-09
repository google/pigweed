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
    "tool",
    "variable_with_value",
)
load(
    "//cc_toolchain/private:providers.bzl",
    "ToolchainFeatureInfo",
)
load(
    "//cc_toolchain/private:utils.bzl",
    "ALL_ASM_ACTIONS",
    "ALL_CPP_COMPILER_ACTIONS",
    "ALL_C_COMPILER_ACTIONS",
    "ALL_LINK_ACTIONS",
    "check_deps",
)

# TODO(b/301004620): Remove when bazel 7 is released and this constant exists in
# ACTION_NAMES
OBJ_COPY_ACTION_NAME = "objcopy_embed_data"
OBJ_DUMP_ACTION_NAME = "objdump_embed_data"

PW_CC_TOOLCHAIN_CONFIG_ATTRS = {
    "feature_deps": "pw_cc_toolchain_feature labels that provide features for this toolchain",
    "ar": "Path to the tool to use for ar (static link) actions",
    "cpp": "Path to the tool to use for C++ compile actions",
    "gcc": "Path to the tool to use for C compile actions",
    "gcov": "Pah to the tool to use for generating code coverag data",
    "ld": "Path to the tool to use for link actions",
    "strip": "Path to the tool to use for strip actions",
    "objcopy": "Path to the tool to use for objcopy actions",
    "objdump": "Path to the tool to use for objdump actions",

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
}

PW_CC_TOOLCHAIN_SHARED_ATTRS = ["toolchain_identifier"]

PW_CC_TOOLCHAIN_BLOCKED_ATTRS = {
    "toolchain_config": "pw_cc_toolchain includes a generated toolchain config",
    "artifact_name_patterns": "pw_cc_toolchain does not yet support artifact name patterns",
    "features": "Use feature_deps to add pw_cc_toolchain_feature deps to the toolchain",
    "action_configs": "pw_cc_toolchain does not yet support action configs, use the \"ar\", \"cpp\", \"gcc\", \"gcov\", \"ld\", and \"strip\" attributes to set toolchain tools",
    "cxx_builtin_include_directories": "Use a pw_cc_toolchain_feature to add cxx_builtin_include_directories",
    "tool_paths": "pw_cc_toolchain does not support tool_paths, use \"ar\", \"cpp\", \"gcc\", \"gcov\", \"ld\", and \"strip\" attributes to set toolchain tools",
    "make_variables": "pw_cc_toolchain does not yet support make variables",
    "builtin_sysroot": "Use a pw_cc_toolchain_feature to add a builtin_sysroot",
}

def _action_configs(action_tool, action_list):
    """Binds a tool to an action.

    Args:
        action_tool (File): Tool to bind to the specified actions.
        action_list (List[str]): List of actions to bind to the specified tool.

    Returns:
        action_config: A action_config binding the provided tool to the
          specified actions.
    """
    return [
        action_config(
            action_name = action,
            tools = [
                tool(
                    tool = action_tool,
                ),
            ],
        )
        for action in action_list
    ]

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
    check_deps(ctx)
    all_actions = []
    all_actions += _action_configs(ctx.executable.gcc, ALL_ASM_ACTIONS)
    all_actions += _action_configs(ctx.executable.gcc, ALL_C_COMPILER_ACTIONS)
    all_actions += _action_configs(ctx.executable.cpp, ALL_CPP_COMPILER_ACTIONS)
    all_actions += _action_configs(ctx.executable.cpp, ALL_LINK_ACTIONS)

    all_actions += [
        action_config(
            action_name = ACTION_NAMES.cpp_link_static_library,
            implies = ["archiver_flags", "linker_param_file"],
            tools = [
                tool(
                    tool = ctx.executable.ar,
                ),
            ],
        ),
        action_config(
            action_name = ACTION_NAMES.llvm_cov,
            tools = [
                tool(
                    tool = ctx.executable.gcov,
                ),
            ],
        ),
        action_config(
            action_name = OBJ_COPY_ACTION_NAME,
            tools = [
                tool(
                    tool = ctx.executable.objcopy,
                ),
            ],
        ),
        action_config(
            action_name = OBJ_DUMP_ACTION_NAME,
            tools = [
                tool(
                    tool = ctx.executable.objdump,
                ),
            ],
        ),
        action_config(
            action_name = ACTION_NAMES.strip,
            tools = [
                tool(
                    tool = ctx.executable.strip,
                ),
            ],
        ),
    ]

    features = [dep[ToolchainFeatureInfo].feature for dep in ctx.attr.feature_deps]
    features.append(_archiver_flags_feature(ctx.attr.target_libc == "macosx"))
    builtin_include_dirs = []
    for dep in ctx.attr.feature_deps:
        builtin_include_dirs.extend(dep[ToolchainFeatureInfo].cxx_builtin_include_directories)

    sysroot_dir = None
    for dep in ctx.attr.feature_deps:
        dep_sysroot = dep[ToolchainFeatureInfo].builtin_sysroot
        if dep_sysroot:
            if sysroot_dir:
                fail("Failed to set sysroot at `{}`, already have sysroot at `{}` ".format(dep_sysroot, sysroot_dir))
            sysroot_dir = dep_sysroot

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        action_configs = all_actions,
        features = features,
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
        "feature_deps": attr.label_list(),
        "gcc": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "ld": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "ar": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "cpp": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "gcov": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "objcopy": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "objdump": attr.label(allow_single_file = True, executable = True, cfg = "exec"),
        "strip": attr.label(allow_single_file = True, executable = True, cfg = "exec"),

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

    Returns
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

def pw_cc_toolchain(**kwargs):
    """A bound cc_toolchain and pw_cc_toolchain_config pair.

    Args:
        **kwargs: All attributes supported by cc_toolchain and pw_cc_toolchain_config.
    """

    _check_args(native.package_relative_label(kwargs["name"]), kwargs)

    cc_toolchain_config_args, cc_toolchain_args = _split_args(kwargs, PW_CC_TOOLCHAIN_CONFIG_ATTRS)

    # Bind pw_cc_toolchain_config and the cc_toolchain.
    config_name = "{}_config".format(cc_toolchain_args["name"])
    cc_toolchain_config_args["name"] = config_name
    cc_toolchain_args["toolchain_config"] = ":{}".format(config_name)

    # Copy over arguments that should be shared by both rules.
    for arg_name in PW_CC_TOOLCHAIN_SHARED_ATTRS:
        if arg_name in cc_toolchain_config_args:
            cc_toolchain_args[arg_name] = cc_toolchain_config_args[arg_name]

    pw_cc_toolchain_config(**cc_toolchain_config_args)
    native.cc_toolchain(**cc_toolchain_args)
