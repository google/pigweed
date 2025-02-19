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
"""Implements the pw_cc_size_binary macro."""

load("//pw_bloat/private:pw_bloat_report.bzl", "PwSizeBinaryInfo")
load("//pw_build:pw_cc_binary.bzl", "pw_cc_binary")

def pw_cc_size_binary(
        name,
        platform = "//pw_bloat:pw_size_report_platform",
        **cc_binary_kwargs):
    """Compiles a pw_cc_binary target with a configured platform.

    By default, this uses the globally-configured
    //pw_bloat:pw_size_report_platform alias to compile the binary. However,
    this may be overridden on a per-binary basis.

    This produces two sub-targets:
      - {name}.base: A pw_cc_binary configured with the provided cc_binary_kwargs.
      - {name}: The same binary built with the configured platform.
    """
    cc_binary_name = name + ".base"

    pw_cc_binary(
        name = cc_binary_name,
        target_compatible_with = select({
            # As these binaries are intended to be built for a size reporting
            # target, they should not be included in sanitizer builds.
            "//pw_toolchain/host_clang:asan_enabled": ["@platforms//:incompatible"],
            "//conditions:default": [],
        }),
        **cc_binary_kwargs
    )

    _pw_platform_size_binary(
        name = name,
        platform = platform,
        target = ":" + cc_binary_name,
    )

def _pw_target_platform_transition_impl(_, attr):
    return {
        "//command_line_option:platforms": str(attr.platform),
    }

_pw_target_platform_transition = transition(
    implementation = _pw_target_platform_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
    ],
)

def _pw_platform_size_binary_impl(ctx):
    linked_executable = ctx.actions.declare_file(ctx.attr.name)

    ctx.actions.symlink(
        output = linked_executable,
        target_file = ctx.executable.target,
        is_executable = True,
    )

    return [
        DefaultInfo(
            executable = linked_executable,
        ),
        PwSizeBinaryInfo(
            binary = linked_executable,
            bloaty_config = ctx.file._bloaty_config,
        ),
    ]

_pw_platform_size_binary = rule(
    implementation = _pw_platform_size_binary_impl,
    attrs = {
        "platform": attr.label(
            mandatory = True,
        ),
        "target": attr.label(
            mandatory = True,
            executable = True,
            cfg = _pw_target_platform_transition,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_bloaty_config": attr.label(
            default = Label("//pw_bloat:pw_size_report_bloaty_config"),
            allow_single_file = True,
            cfg = _pw_target_platform_transition,
        ),
    },
    executable = True,
)
