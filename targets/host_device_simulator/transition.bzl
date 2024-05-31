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

"""Bazel transitions for the host device simulator."""

def _host_device_simulator_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = settings, attr
    return {
        "@pigweed//pw_log:backend": "@pigweed//pw_log_string",
        "@pigweed//pw_log:backend_impl": "@pigweed//pw_log_string:impl",
        "@pigweed//pw_log_string:handler_backend": "@pigweed//pw_system:log_backend",
        "@pigweed//pw_sys_io:backend": "@pigweed//pw_sys_io_stdio",
        "@pigweed//pw_system:io_backend": "@pigweed//pw_system:socket_target_io",
    }

_host_device_simulator_transition = transition(
    implementation = _host_device_simulator_transition_impl,
    inputs = [],
    outputs = [
        "@pigweed//pw_log:backend",
        "@pigweed//pw_log:backend_impl",
        "@pigweed//pw_log_string:handler_backend",
        "@pigweed//pw_sys_io:backend",
        "@pigweed//pw_system:io_backend",
    ],
)

def _host_device_simulator_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(output = out, target_file = ctx.executable.binary)
    return [DefaultInfo(files = depset([out]), executable = out)]

host_device_simulator_binary = rule(
    _host_device_simulator_binary_impl,
    attrs = {
        "binary": attr.label(
            doc = "cc_binary build for the host_device_simulator",
            cfg = _host_device_simulator_transition,
            executable = True,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    doc = "Builds the specified binary for the host_device_simulator platform",
)
