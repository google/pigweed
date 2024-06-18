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

# From the rp2040 config:
# build:rp2040 --platforms=@pigweed//targets/rp2040
# build:rp2040 --//pw_log:backend=@pigweed//pw_log_tokenized
# build:rp2040 --//pw_log:backend_impl=@pigweed//pw_log_tokenized:impl
# build:rp2040 --//pw_log_tokenized:handler_backend=@pigweed//pw_system:log_backend
# build:rp2040 --//pw_system:extra_platform_libs=@pigweed//targets/rp2040:extra_platform_libs
# build:rp2040 --//pw_unit_test:main=//targets/rp2040:unit_test_app
# build:rp2040 --@pico-sdk//bazel/config:PICO_STDIO_USB=True
# build:rp2040 --@pico-sdk//bazel/config:PICO_STDIO_UART=True

def _rp2040_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = settings, attr
    return {
        "//command_line_option:platforms": "@pigweed//targets/rp2040",
        "@pico-sdk//bazel/config:PICO_STDIO_UART": True,
        "@pico-sdk//bazel/config:PICO_STDIO_USB": True,
        "@pigweed//pw_log:backend": "@pigweed//pw_log_tokenized",
        "@pigweed//pw_log:backend_impl": "@pigweed//pw_log_tokenized:impl",
        "@pigweed//pw_log_tokenized:handler_backend": "@pigweed//pw_system:log_backend",
        "@pigweed//pw_system:extra_platform_libs": "@pigweed//targets/rp2040:extra_platform_libs",
        "@pigweed//pw_unit_test:main": "@pigweed//targets/rp2040:unit_test_app",
    }

_rp2040_transition = transition(
    implementation = _rp2040_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
        "@pigweed//pw_log:backend",
        "@pigweed//pw_log:backend_impl",
        "@pigweed//pw_log_tokenized:handler_backend",
        "@pigweed//pw_system:extra_platform_libs",
        "@pigweed//pw_unit_test:main",
        "@pico-sdk//bazel/config:PICO_STDIO_USB",
        "@pico-sdk//bazel/config:PICO_STDIO_UART",
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
    executable = True,
)
