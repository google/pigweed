#!/usr/bin/env python3

# Copyright 2022 The Pigweed Authors
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
""" An internal set of tools for creating embedded CC targets. """

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@rules_cc//cc:action_names.bzl", "C_COMPILE_ACTION_NAME")

DEBUGGING = [
    "-g",
]

# Standard compiler flags to reduce output binary size.
REDUCED_SIZE_COPTS = [
    "-fno-common",
    "-fno-exceptions",
    "-ffunction-sections",
    "-fdata-sections",
]

STRICT_WARNINGS_COPTS = [
    "-Wall",
    "-Wextra",
    # Make all warnings errors, except for the exemptions below.
    "-Werror",
    "-Wno-error=cpp",  # preprocessor #warning statement
    "-Wno-error=deprecated-declarations",  # [[deprecated]] attribute
]

PW_DEFAULT_COPTS = (
    DEBUGGING +
    REDUCED_SIZE_COPTS +
    STRICT_WARNINGS_COPTS
)

KYTHE_COPTS = [
    "-Wno-unknown-warning-option",
]

def add_defaults(kwargs):
    """Adds default arguments suitable for both C and C++ code to kwargs.

    Args:
        kwargs: cc_* arguments to be modified.
    """

    copts = PW_DEFAULT_COPTS + kwargs.get("copts", [])
    kwargs["copts"] = select({
        "@pigweed//pw_build:kythe": copts + KYTHE_COPTS,
        "//conditions:default": copts,
    })

    # Set linkstatic to avoid building .so files.
    kwargs["linkstatic"] = True

    kwargs.setdefault("features", [])

    # Crosstool--adding this line to features disables header modules, which
    # don't work with -fno-rtti. Note: this is not a command-line argument,
    # it's "minus use_header_modules".
    kwargs["features"].append("-use_header_modules")

def _preprocess_linker_script_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)
    output_script = ctx.actions.declare_file(ctx.label.name + ".ld")
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )
    cxx_compiler_path = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
    )
    c_compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        user_compile_flags = ctx.fragments.cpp.copts + ctx.fragments.cpp.conlyopts,
    )
    env = cc_common.get_environment_variables(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = c_compile_variables,
    )
    ctx.actions.run(
        outputs = [output_script],
        inputs = depset(
            [ctx.file.linker_script],
            transitive = [cc_toolchain.all_files],
        ),
        executable = cxx_compiler_path,
        arguments = [
            "-E",
            "-P",
            # TODO: b/296928739 - This flag is needed so cc1 can be located
            # despite the presence of symlinks. Normally this is provided
            # through copts inherited from the toolchain, but since those are
            # not pulled in here the flag must be explicitly added.
            "-no-canonical-prefixes",
            "-xc",
            ctx.file.linker_script.short_path,
            "-o",
            output_script.path,
        ] + [
            "-D" + d
            for d in ctx.attr.defines
        ] + ctx.attr.copts,
        env = env,
    )
    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        user_link_flags = ["-T", output_script.path],
        additional_inputs = depset(direct = [output_script]),
    )
    linking_context = cc_common.create_linking_context(
        linker_inputs = depset(direct = [linker_input]),
    )
    return [
        DefaultInfo(files = depset([output_script])),
        CcInfo(linking_context = linking_context),
    ]

pw_linker_script = rule(
    _preprocess_linker_script_impl,
    attrs = {
        "copts": attr.string_list(doc = "C compile options."),
        "defines": attr.string_list(doc = "C preprocessor defines."),
        "linker_script": attr.label(
            mandatory = True,
            allow_single_file = True,
            doc = "Linker script to preprocess.",
        ),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
)

def _print_platform_impl(_, ctx):
    if hasattr(ctx.rule.attr, "constraint_values"):
        for cv in ctx.rule.attr.constraint_values:
            # buildifier: disable=print
            print(str(ctx.rule.attr.name) + " specifies " + str(cv))
    return []

print_platform = aspect(
    implementation = _print_platform_impl,
    attr_aspects = ["parents"],
    doc = """
        This is a little debug utility that traverses the platform inheritance
        hierarchy and prints all the constraint values.

        Example usage:

        bazel build \
          //pw_build/platforms:lm3s6965evb \
          --aspects \
          pw_build/bazel_internal/pigweed_internal.bzl%print_platform
    """,
)
