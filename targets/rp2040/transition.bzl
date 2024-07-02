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

# LINT.IfChange
def _rp2040_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = settings, attr
    return {
        "//command_line_option:platforms": "@pigweed//targets/rp2040",
        "@freertos//:freertos_config": "@pigweed//targets/rp2040:freertos_config",
        "@pico-sdk//bazel/config:PICO_STDIO_UART": True,
        "@pico-sdk//bazel/config:PICO_STDIO_USB": True,
        "@pigweed//pw_assert:backend": "@pigweed//pw_assert_basic",
        "@pigweed//pw_assert:backend_impl": "@pigweed//pw_assert_basic:impl",
        "@pigweed//pw_assert:check_backend": "@pigweed//pw_assert_basic",
        "@pigweed//pw_assert:check_backend_impl": "@pigweed//pw_assert_basic:impl",
        "@pigweed//pw_interrupt:backend": "@pigweed//pw_interrupt_cortex_m:context",
        "@pigweed//pw_log:backend": "@pigweed//pw_log_tokenized",
        "@pigweed//pw_log:backend_impl": "@pigweed//pw_log_tokenized:impl",
        "@pigweed//pw_log_tokenized:handler_backend": "@pigweed//pw_system:log_backend",
        "@pigweed//pw_sync:binary_semaphore_backend": "@pigweed//pw_sync_freertos:binary_semaphore",
        "@pigweed//pw_sync:interrupt_spin_lock_backend": "@pigweed//pw_sync_freertos:interrupt_spin_lock",
        "@pigweed//pw_sync:mutex_backend": "@pigweed//pw_sync_freertos:mutex",
        "@pigweed//pw_sync:thread_notification_backend": "@pigweed//pw_sync_freertos:thread_notification",
        "@pigweed//pw_sync:timed_thread_notification_backend": "@pigweed//pw_sync_freertos:timed_thread_notification",
        "@pigweed//pw_sys_io:backend": "@pigweed//pw_sys_io_rp2040",
        "@pigweed//pw_system:extra_platform_libs": "@pigweed//targets/rp2040:extra_platform_libs",
        "@pigweed//pw_thread:id_backend": "@pigweed//pw_thread_freertos:id",
        "@pigweed//pw_thread:iteration_backend": "@pigweed//pw_thread_freertos:thread_iteration",
        "@pigweed//pw_thread:sleep_backend": "@pigweed//pw_thread_freertos:sleep",
        "@pigweed//pw_thread:thread_backend": "@pigweed//pw_thread_freertos:thread",
        "@pigweed//pw_unit_test:main": "@pigweed//targets/rp2040:unit_test_app",
    }

# LINT.ThenChange(//.bazelrc)

_rp2040_transition = transition(
    implementation = _rp2040_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
        "@freertos//:freertos_config",
        "@pico-sdk//bazel/config:PICO_STDIO_USB",
        "@pico-sdk//bazel/config:PICO_STDIO_UART",
        "@pigweed//pw_assert:backend",
        "@pigweed//pw_assert:backend_impl",
        "@pigweed//pw_assert:check_backend",
        "@pigweed//pw_assert:check_backend_impl",
        "@pigweed//pw_interrupt:backend",
        "@pigweed//pw_log:backend",
        "@pigweed//pw_log:backend_impl",
        "@pigweed//pw_log_tokenized:handler_backend",
        "@pigweed//pw_sync:binary_semaphore_backend",
        "@pigweed//pw_sync:interrupt_spin_lock_backend",
        "@pigweed//pw_sync:mutex_backend",
        "@pigweed//pw_sync:thread_notification_backend",
        "@pigweed//pw_sync:timed_thread_notification_backend",
        "@pigweed//pw_sys_io:backend",
        "@pigweed//pw_system:extra_platform_libs",
        "@pigweed//pw_thread:id_backend",
        "@pigweed//pw_thread:sleep_backend",
        "@pigweed//pw_thread:thread_backend",
        "@pigweed//pw_thread:iteration_backend",
        "@pigweed//pw_unit_test:main",
    ],
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
