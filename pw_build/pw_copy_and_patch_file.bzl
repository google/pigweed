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
"""Implementation for pw_copy_and_patch_file"""

load("@bazel_skylib//rules:run_binary.bzl", "run_binary")

def pw_copy_and_patch_file(*, name, src, out, patch_file, **kwargs):
    """Applies a patch to a copy of the file.

    The source file will not be patched in place, but instead copied before
    patching.  The output of this target will be the patched file.

    Args:
      name: The name of the target.
      src: The source file to be patched.
      out: The output file containing the patched contents. This value
           can not be the same as the src value.
      patch_file: The patch file.
      **kwargs: Passed to run_binary.
    """
    _args = [
        "--patch-file",
        "$(execpath " + patch_file + ")",
        "--src",
        "$(execpath " + src + ")",
        "--dst",
        "$(execpath " + out + ")",
    ]

    # If src is an external repo, set the root
    # of the external repo path.
    if Label(src).workspace_root:
        root = Label(src).workspace_root
        _args.append("--root")
        _args.append(root)

    run_binary(
        name = name,
        srcs = [src, patch_file],
        outs = [out],
        args = _args,
        tool = str(Label("//pw_build/py:copy_and_patch")),
        **kwargs
    )
