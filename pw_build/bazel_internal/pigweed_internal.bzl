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

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

KYTHE_COPTS = [
    "-Wno-unknown-warning-option",
]

def add_defaults(kwargs):
    """Adds default arguments suitable for both C and C++ code to kwargs.

    Args:
        kwargs: cc_* arguments to be modified.
    """

    copts = kwargs.get("copts", [])
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

def compile_cc(
        ctx,
        srcs,
        hdrs,
        deps,
        includes = [],
        defines = [],
        user_compile_flags = []):
    """Compiles a list C++ source files.

    Args:
        ctx: Rule context
        srcs: List of C/C++ source files to compile
        hdrs: List of C/C++ header files to compile with
        deps: Dependencies to link with
        includes: List of include paths
        defines: List of preprocessor defines to use
        user_compile_flags: Extra compiler flags to pass when compiling.

    Returns:
      A CcInfo provider.
    """
    cc_toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )

    compilation_contexts = [dep[CcInfo].compilation_context for dep in deps]
    compilation_context, compilation_outputs = cc_common.compile(
        name = ctx.label.name,
        actions = ctx.actions,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        srcs = srcs,
        includes = includes,
        defines = defines,
        public_hdrs = hdrs,
        user_compile_flags = user_compile_flags,
        compilation_contexts = compilation_contexts,
    )

    linking_contexts = [dep[CcInfo].linking_context for dep in deps]
    linking_context, _ = cc_common.create_linking_context_from_compilation_outputs(
        actions = ctx.actions,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        compilation_outputs = compilation_outputs,
        linking_contexts = linking_contexts,
        disallow_dynamic_library = True,
        name = ctx.label.name,
    )

    transitive_output_files = [dep[DefaultInfo].files for dep in deps]
    output_files = depset(
        compilation_outputs.pic_objects + compilation_outputs.objects,
        transitive = transitive_output_files,
    )
    return [DefaultInfo(files = output_files), CcInfo(
        compilation_context = compilation_context,
        linking_context = linking_context,
    )]
