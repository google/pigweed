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
"""Generates an Xcode repository that enables //features/macos:macos_sysroot."""

load(
    "//features/macos/private:xcode_command_line_tools.bzl",
    "pw_xcode_repository",
)

def pw_xcode_command_line_tools_repository():
    """Call this from a WORKSPACE file."""
    pw_xcode_repository(
        name = "pw_xcode_command_line_tools",
    )
