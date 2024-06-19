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

"""Substitute a template file with variables from bazel workspace status."""

def pw_substitute_workspace_status(name, src, out):
    native.genrule(
        name = name,
        srcs = [src],
        outs = [out],
        # bazel-out/stable-status.txt and bazel-out/volatile-status.txt
        # are made available by setting stamp = True.
        cmd = "$(location @pigweed//pw_build_info:substitute_workspace_status_tool)" +
              " --status-file bazel-out/volatile-status.txt" +
              " --status-file bazel-out/stable-status.txt" +
              " $< $@",
        stamp = True,
        tools = ["@pigweed//pw_build_info:substitute_workspace_status_tool"],
    )
