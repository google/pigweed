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
"""A basic extension for exposing a macOS host's local Xcode libraries."""

def _get_xcode_sdk_root(extension_ctx):
    xcrun_result = extension_ctx.execute(["/usr/bin/xcrun", "--show-sdk-path"])
    if xcrun_result.return_code != 0:
        fail("Failed locating Xcode SDK: {}".format(xcrun_result.stderr))
    return xcrun_result.stdout.replace("\n", "")

# If you squint hard enough, this looks like a new_local_repository, but
# we very intentionally avoid using new_local_repository here since the xcode
# SDK path gets baked into the MODULE.bazel.lock which breaks CI/CQ bots.
#
# See https://issues.pigweed.dev/issues/346388161#comment24 for more details.
def _xcode_repository_impl(repository_ctx):
    if repository_ctx.os.name != "mac os x":
        fail(
            "Cannot expose a local Xcode SDK on {} hosts. This repository " +
            "rule only works on macOS hosts. It's possible a rule from one " +
            "of your build files is referencing this repository and is not " +
            "properly gated behind a `target_compatible_with` expression or " +
            "`select()` statement." % (repository_ctx.os,),
        )
    p = repository_ctx.path(_get_xcode_sdk_root(repository_ctx))
    children = p.readdir()
    for child in children:
        repository_ctx.symlink(child, child.basename)
    repository_ctx.symlink(repository_ctx.attr.build_file, "BUILD")

xcode_sdk_repository = repository_rule(
    implementation = _xcode_repository_impl,
    attrs = {
        "build_file": attr.label(
            allow_single_file = True,
        ),
    },
    local = True,
    configure = True,
)
