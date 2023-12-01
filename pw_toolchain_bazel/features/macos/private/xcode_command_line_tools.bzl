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

def _pw_xcode_repository_impl(repository_ctx):
    """Generated repository containing a pw_xcode_info target.

    Args:
        repository_ctx: The context of the current repository.

    Returns:
        None
    """

    build_file_contents = [
        "package(default_visibility = [\"//visibility:public\"])",
        "",
        "filegroup(",
        "    name = \"default\",",
        "    visibility = [\"//visibility:private\"],",
        ")",
    ]
    repository_ctx.file("BUILD", "\n".join(build_file_contents))

    # Generate a stub and early return if not on macOS.
    if repository_ctx.os.name != "mac os x":
        # Generate the constant, but make it empty.
        defs_file_contents = [
            "XCODE_SDK_PATH = \"\"",
        ]
        repository_ctx.file("defs.bzl", "\n".join(defs_file_contents))
        return

    xcrun_result = repository_ctx.execute(["/usr/bin/xcrun", "--show-sdk-path"])
    if xcrun_result.return_code != 0:
        fail("Failed locating Xcode SDK: {}".format(xcrun_result.stderr))

    sdk_path = xcrun_result.stdout.replace("\n", "")
    defs_file_contents = [
        "XCODE_SDK_PATH = \"{}\"".format(sdk_path),
    ]
    repository_ctx.file("defs.bzl", "\n".join(defs_file_contents))

pw_xcode_repository = repository_rule(
    _pw_xcode_repository_impl,
    attrs = {},
    doc = "Initializes a macOS SDK repository",
)
