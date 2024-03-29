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

# TODO: https://github.com/bazelbuild/bazel/issues/16546 - Use
# cc_helper.is_compilation_outputs_empty() if/when it's available for
# end-users.
def _is_compilation_outputs_empty(compilation_outputs):
    return (len(compilation_outputs.pic_objects) == 0 and
            len(compilation_outputs.objects) == 0)

def compile_cc(
        ctx,
        srcs,
        hdrs,
        deps,
        includes = [],
        defines = [],
        local_defines = [],
        user_compile_flags = [],
        alwayslink = False):
    """Compiles a list C++ source files.

    Args:
        ctx: Rule context
        srcs: List of C/C++ source files to compile
        hdrs: List of C/C++ header files to compile with
        deps: Dependencies to link with
        includes: List of include paths
        defines: List of preprocessor defines to use
        local_defines: List of preprocessor defines to use only on this unit.
        user_compile_flags: Extra compiler flags to pass when compiling.
        alwayslink: Whether this library should always be linked.

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
        local_defines = local_defines,
        public_hdrs = hdrs,
        user_compile_flags = user_compile_flags,
        compilation_contexts = compilation_contexts,
    )

    linking_contexts = [dep[CcInfo].linking_context for dep in deps]

    # If there's no compiled artifacts (i.e. the library is header-only), don't
    # try and link a library.
    #
    # TODO: https://github.com/bazelbuild/bazel/issues/18095 - Remove this
    # if create_linking_context_from_compilation_outputs() is changed to no
    # longer require this workaround.
    if not _is_compilation_outputs_empty(compilation_outputs):
        linking_context, link_outputs = cc_common.create_linking_context_from_compilation_outputs(
            actions = ctx.actions,
            feature_configuration = feature_configuration,
            cc_toolchain = cc_toolchain,
            compilation_outputs = compilation_outputs,
            linking_contexts = linking_contexts,
            disallow_dynamic_library = True,
            alwayslink = alwayslink,
            name = ctx.label.name,
        )

        if link_outputs.library_to_link != None:
            linking_contexts.append(linking_context)

    return [
        CcInfo(
            compilation_context = compilation_context,
            linking_context = cc_common.merge_linking_contexts(
                linking_contexts = linking_contexts,
            ),
        ),
    ]

# From cc_helper.bzl. Feature names for static/dynamic linking.
linker_mode = struct(
    LINKING_DYNAMIC = "dynamic_linking_mode",
    LINKING_STATIC = "static_linking_mode",
)

def link_cc(
        ctx,
        linking_contexts,
        linkstatic,
        stamp,
        user_link_flags = [],
        additional_outputs = []):
    """Links a binary and allows custom linker output.

    Args:
        ctx: Rule context
        linking_contexts: Dependencies to link with
        linkstatic: True if binary should be linked statically.
        stamp: Stamp behavior for linking.
        user_link_flags: Extra linker flags to pass when linking
        additional_outputs: Extra files generated by link

    Returns:
      DefaultInfo of output files
    """
    cc_toolchain = find_cpp_toolchain(ctx)
    features = ctx.features
    linking_mode = linker_mode.LINKING_STATIC
    if not linkstatic:
        linking_mode = linker_mode.LINKING_DYNAMIC
    features.append(linking_mode)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = features,
        unsupported_features = ctx.disabled_features,
    )

    linking_outputs = cc_common.link(
        name = ctx.label.name,
        actions = ctx.actions,
        stamp = stamp,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        linking_contexts = linking_contexts,
        user_link_flags = user_link_flags,
        output_type = "executable",
        additional_outputs = additional_outputs,
    )

    output_files = depset(
        [linking_outputs.executable] + additional_outputs,
        transitive = [],
    )
    return [DefaultInfo(files = output_files)]
