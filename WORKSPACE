# Copyright 2021 The Pigweed Authors
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

workspace(
    name = "pigweed",
)

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# TODO: b/383856665 - rules_fuchsia requires host_platform. Once this is fixed
# we can remove this entry.
load("@platforms//host:extension.bzl", "host_platform_repo")

host_platform_repo(
    name = "host_platform",
)

# Setup Fuchsia SDK.
git_repository(
    name = "fuchsia_infra",
    # ROLL: Warning: this entry is automatically updated.
    # ROLL: Last updated 2025-01-25.
    # ROLL: By https://cr-buildbucket.appspot.com/build/8724735070800369633.
    commit = "124a9b90520a48e9076d6167f3abfb55ce0aa9aa",
    remote = "https://fuchsia.googlesource.com/fuchsia-infra-bazel-rules",
)

# fuchsia_infra_workspace is a macro, not a repository rule, so we can't call
# it from MODULE.bazel.
load("@fuchsia_infra//:workspace.bzl", "fuchsia_infra_workspace")

fuchsia_infra_workspace()

# Required by fuzztest
http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "f89c61410a072e5cbcf8c27e3a778da7d6fd2f2b5b1445cd4f4508bee946ab0f",
    strip_prefix = "re2-2022-06-01",
    url = "https://github.com/google/re2/archive/refs/tags/2022-06-01.tar.gz",
)

# Required by fuzztest
http_archive(
    name = "com_google_absl",
    sha256 = "338420448b140f0dfd1a1ea3c3ce71b3bc172071f24f4d9a57d59b45037da440",
    strip_prefix = "abseil-cpp-20240116.0",
    url = "https://github.com/abseil/abseil-cpp/releases/download/20240116.0/abseil-cpp-20240116.0.tar.gz",
)

# Required by fuzztest
http_archive(
    name = "rules_proto",
    sha256 = "14a225870ab4e91869652cfd69ef2028277fc1dc4910d65d353b62d6e0ae21f4",
    strip_prefix = "rules_proto-7.1.0",
    url = "https://github.com/bazelbuild/rules_proto/releases/download/7.1.0/rules_proto-7.1.0.tar.gz",
)

# TODO: https://pwbug.dev/365103864 - Fuzztest is not in the BCR yet (also see
# https://github.com/google/fuzztest/issues/950).
http_archive(
    name = "com_google_fuzztest",
    strip_prefix = "fuzztest-6eb010c7223a6aa609b94d49bfc06ac88f922961",
    url = "https://github.com/google/fuzztest/archive/6eb010c7223a6aa609b94d49bfc06ac88f922961.zip",
)
