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
"""Xcode configuration for Bazel build.

This replaces xcode_configure, but only intends to work with macOS host builds,
and exclusively attempts to support xcode command-line tools.
"""

load("//cc_toolchain/private:providers.bzl", "ToolchainFeatureInfo")  # buildifier: disable=bzl-visibility
load("//cc_toolchain/private:toolchain_feature.bzl", "build_toolchain_feature_info")  # buildifier: disable=bzl-visibility

XcodeSdkInfo = provider(
    doc = "A simple provider that provides the path to the macOS Xcode SDK",
    fields = {
        "sdk_path": "str: Path to the macOS sdk",
    },
)

def _pw_xcode_repository_impl(repository_ctx):
    """Generated repository containing a pw_xcode_info target.

    Args:
        repository_ctx: The context of the current repository.

    Returns:
        None
    """

    # This is required to make a repository, so make a stub for all other
    # operating systems.
    if repository_ctx.os.name != "mac os x":
        lines = [
            "filegroup(",
            "    name = \"default\",",
            "    visibility = [\"@pw_toolchain//features/macos:__pkg__\"],",
            ")",
        ]
        repository_ctx.file("BUILD", "\n".join(lines))
        return

    xcrun_result = repository_ctx.execute(["/usr/bin/xcrun", "--show-sdk-path"])
    if xcrun_result.return_code != 0:
        fail("Failed locating Xcode SDK: {}".format(xcrun_result.stderr))

    sdk_path = xcrun_result.stdout.replace("\n", "")
    lines = [
        "load(\"@pw_toolchain//features/macos/private:xcode_command_line_tools.bzl\", \"pw_xcode_info\")",
        "pw_xcode_info(",
        "    name = \"default\",",
        "    sdk_path = \"{}\",".format(sdk_path),
        "    visibility = [\"@pw_toolchain//features/macos:__pkg__\"],",
        ")",
    ]

    if xcrun_result.return_code == 0:
        repository_ctx.file("BUILD", "\n".join(lines))

pw_xcode_repository = repository_rule(
    _pw_xcode_repository_impl,
    attrs = {},
    doc = "Initializes a macOS SDK repository",
)

def _xcode_info_impl(ctx):
    """Rule that provides XcodeSdkInfo.

    Args:
        ctx: The context of the current build rule.

    Returns:
        XcodeSdkInfo
    """
    return [XcodeSdkInfo(sdk_path = ctx.attr.sdk_path)]

pw_xcode_info = rule(
    implementation = _xcode_info_impl,
    attrs = {
        "sdk_path": attr.string(),
    },
    provides = [XcodeSdkInfo],
)

def _pw_macos_sysroot_impl(ctx):
    """Rule that provides an Xcode-provided sysroot as ToolchainFeatureInfo.

    Args:
        ctx: The context of the current build rule.

    Returns:
        ToolchainFeatureInfo
    """
    sdk_path = ctx.attr.sdk[XcodeSdkInfo].sdk_path
    return build_toolchain_feature_info(
        ctx = ctx,
        cxx_builtin_include_directories = ["%sysroot%/usr/include"],
        builtin_sysroot = sdk_path,
    )

pw_macos_sysroot = rule(
    implementation = _pw_macos_sysroot_impl,
    attrs = {
        "sdk": attr.label(),
    },
    provides = [ToolchainFeatureInfo],
)
