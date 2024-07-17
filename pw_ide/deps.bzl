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

"""Contains the external dependencies for pw_ide."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def pw_ide_deps():
    """Instantiates dependencies used by pw_ide."""

    # Hedron's Compile Commands Extractor for Bazel
    # https://github.com/hedronvision/bazel-compile-commands-extractor
    # This commit is HEAD at the time this file is committed.
    maybe(
        http_archive,
        name = "hedron_compile_commands",
        # TODO: https://pwbug.dev/349880767 - Point this back to the upstream
        # repo once this PR is merged.
        url = "https://github.com/chadnorvell/bazel-compile-commands-extractor/archive/163521345aa6366fd1ed801b989b668b5c806f69.tar.gz",
        strip_prefix = "bazel-compile-commands-extractor-163521345aa6366fd1ed801b989b668b5c806f69",
        sha256 = "b701c8f37d343ef5fd7938868b5e1de7eaad35a275c45d235bfe924a0d43ef7f",
    )
