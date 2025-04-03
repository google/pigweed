# Copyright 2025 The Pigweed Authors
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

load("@pigweed//pw_build:pw_py_importable_runfile.bzl", "pw_py_importable_runfile")
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

py_console_script_binary(
    name = "black_entry_point",
    script = "black",
    pkg = "@pigweed_python_packages//black",
)

pw_py_importable_runfile(
    name = "black",
    src = ":black_entry_point",
    executable = True,
    import_location = "pw_build_external_runfile_resource.black",
    visibility = ["@pigweed//:__subpackages__"],
)
