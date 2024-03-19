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

"""Intializes a workspace with information about the host os and arch."""

def _host_metadata_repository_impl(rctx):
    rctx.file("BUILD.bazel", "exports_files(glob(['**/*']))")
    rctx.file("host_metadata.bzl", """
HOST_OS = "%s"
HOST_ARCH = "%s"
""" % (rctx.os.name, rctx.os.arch))

host_metadata_repository = repository_rule(
    implementation = _host_metadata_repository_impl,
    doc = "Intializes a workspace with information about the host os and arch.",
)
