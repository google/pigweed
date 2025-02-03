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
"""
pw_rust_crates_extension allows the pigweed modules "rust_crates" repo to
be overridden using https://bazel.build/rules/lib/globals/module#override_repo

Downstream projects needing to provide a local `rust_crates` repo can
do so by adding the following to their root MODULE.bazel


local_repository(name = "rust_crates", path = "build/crates_io")

pw_rust_crates = use_extension("@pigweed//pw_build:pw_rust_crates_extension.bzl", "pw_rust_crates_extension")
override_repo(pw_rust_crates, rust_crates = "rust_crates")
"""

load("@bazel_tools//tools/build_defs/repo:local.bzl", "local_repository")

def _pw_rust_crates_extension_impl(_ctx):
    local_repository(name = "rust_crates", path = "third_party/crates_io/rust_crates")

pw_rust_crates_extension = module_extension(
    implementation = _pw_rust_crates_extension_impl,
)
