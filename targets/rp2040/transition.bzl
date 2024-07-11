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

"""Bazel transitions for the rp2040."""

load("//pw_build:merge_flags.bzl", "merge_flags_for_transition_impl", "merge_flags_for_transition_outputs")
load("//third_party/freertos:flags.bzl", "FREERTOS_FLAGS")

# LINT.IfChange
# Typical RP2040 pw_system backends and other platform configuration flags.
RP2040_SYSTEM_FLAGS = FREERTOS_FLAGS | {
    "@freertos//:freertos_config": "@pigweed//targets/rp2040:freertos_config",
    "@pico-sdk//bazel/config:PICO_STDIO_UART": True,
    "@pico-sdk//bazel/config:PICO_STDIO_USB": True,
    "@pigweed//pw_assert:check_backend": "@pigweed//pw_assert_basic",
    "@pigweed//pw_assert:check_backend_impl": "@pigweed//pw_assert_basic:impl",
    "@pigweed//pw_cpu_exception:entry_backend": "@pigweed//pw_cpu_exception_cortex_m:cpu_exception",
    "@pigweed//pw_cpu_exception:entry_backend_impl": "@pigweed//pw_cpu_exception_cortex_m:cpu_exception_impl",
    "@pigweed//pw_cpu_exception:handler_backend": "@pigweed//pw_cpu_exception:basic_handler",
    "@pigweed//pw_cpu_exception:support_backend": "@pigweed//pw_cpu_exception_cortex_m:support",
    "@pigweed//pw_interrupt:backend": "@pigweed//pw_interrupt_cortex_m:context",
    "@pigweed//pw_log:backend": "@pigweed//pw_log_tokenized",
    "@pigweed//pw_log:backend_impl": "@pigweed//pw_log_tokenized:impl",
    "@pigweed//pw_log_tokenized:handler_backend": "@pigweed//pw_system:log_backend",
    "@pigweed//pw_sys_io:backend": "@pigweed//pw_sys_io_rp2040",
    "@pigweed//pw_system:extra_platform_libs": "@pigweed//targets/rp2040:extra_platform_libs",
    "@pigweed//pw_unit_test:backend": "@pigweed//pw_unit_test:light",
    "@pigweed//pw_unit_test:main": "@pigweed//targets/rp2040:unit_test_app",
}

# Additional flags specific to the upstream Pigweed RP2040 platform.
_rp2040_flags = {
    "//command_line_option:platforms": "@pigweed//targets/rp2040",
}
# LINT.ThenChange(//.bazelrc)

def _rp2040_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = settings, attr
    return merge_flags_for_transition_impl(base = RP2040_SYSTEM_FLAGS, override = _rp2040_flags)

_rp2040_transition = transition(
    implementation = _rp2040_transition_impl,
    inputs = [],
    outputs = merge_flags_for_transition_outputs(base = RP2040_SYSTEM_FLAGS, override = _rp2040_flags),
)

def _rp2040_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(output = out, is_executable = True, target_file = ctx.executable.binary)
    return [DefaultInfo(files = depset([out]), executable = out)]

rp2040_binary = rule(
    _rp2040_binary_impl,
    attrs = {
        "binary": attr.label(
            doc = "cc_binary build for the rp2040",
            cfg = _rp2040_transition,
            executable = True,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    doc = "Builds the specified binary for the rp2040 platform",
    # This target is for rp2040 and can't be run on host.
    executable = False,
)
