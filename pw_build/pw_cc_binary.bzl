# Copyright 2024 The Pigweed Authors
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
"""Pigweed's customized cc_library wrappers."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "use_cpp_toolchain")
load(
    "//pw_build/bazel_internal:pigweed_internal.bzl",
    _compile_cc = "compile_cc",
    _link_cc = "link_cc",
)

def pw_cc_binary(**kwargs):
    """Wrapper for cc_binary providing some defaults.

    Specifically, this wrapper adds deps on backend_impl targets for pw_assert
    and pw_log.

    Args:
      **kwargs: Passed to cc_binary.
    """
    kwargs["deps"] = kwargs.get("deps", []) + [str(Label("//pw_build:default_link_extra_lib"))]
    native.cc_binary(**kwargs)

def _pw_cc_binary_with_map_impl(ctx):
    [cc_info] = _compile_cc(
        ctx,
        ctx.files.srcs,
        [],
        deps = ctx.attr.deps + [ctx.attr.link_extra_lib, ctx.attr.malloc],
        includes = ctx.attr.includes,
        defines = ctx.attr.defines,
        local_defines = ctx.attr.local_defines,
    )

    map_file = ctx.actions.declare_file(ctx.label.name + ".map")
    map_flags = ["-Wl,-Map=" + map_file.path]

    return _link_cc(
        ctx,
        [cc_info.linking_context],
        ctx.attr.linkstatic,
        ctx.attr.stamp,
        user_link_flags = ctx.attr.linkopts + map_flags,
        additional_outputs = [map_file],
    )

pw_cc_binary_with_map = rule(
    implementation = _pw_cc_binary_with_map_impl,
    doc = """Links a binary like cc_binary does but generates a linker map file
    and provides it as an output after the executable in the DefaultInfo list
    returned by this rule.

    This rule makes an effort to somewhat mimic cc_binary args and behavior but
    doesn't fully support all options currently. Make variable substitution and
    tokenization handling isn't implemented by this rule on any of it's attrs.

    Args:
        ctx: Rule context.
    """,
    attrs = {
        "defines": attr.string_list(
            doc = "List of defines to add to the compile line.",
        ),
        "deps": attr.label_list(
            providers = [CcInfo],
            doc = "The list of other libraries to be linked in to the binary target.",
        ),
        "includes": attr.string_list(
            doc = "List of include dirs to add to the compile line.",
        ),
        "link_extra_lib": attr.label(
            default = "@bazel_tools//tools/cpp:link_extra_lib",
            doc = "Control linking of extra libraries.",
        ),
        "linkopts": attr.string_list(
            doc = "Add these flags to the C++ linker command.",
        ),
        "linkstatic": attr.bool(
            doc = "True if binary should be link statically",
        ),
        "local_defines": attr.string_list(
            doc = "List of defines to add to the compile line.",
        ),
        "malloc": attr.label(
            default = "@bazel_tools//tools/cpp:malloc",
            doc = "Override the default dependency on malloc.",
        ),
        "srcs": attr.label_list(
            allow_files = True,
            doc = "The list of C and C++ files that are processed to create the target.",
        ),
        "stamp": attr.int(
            default = -1,
            doc = "Whether to encode build information into the binary.",
        ),
    },
    executable = True,
    provides = [DefaultInfo],
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
