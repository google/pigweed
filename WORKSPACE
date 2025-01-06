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
load(
    "//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl",
    "cipd_repository",
)

host_platform_repo(
    name = "host_platform",
)

# Setup Fuchsia SDK.
# Required by: bt-host.
# Used in modules: //pw_bluetooth_sapphire.
# NOTE: These blocks cannot feasibly be moved into a macro.
# See https://github.com/bazelbuild/bazel/issues/1550
git_repository(
    name = "fuchsia_infra",
    # ROLL: Warning: this entry is automatically updated.
    # ROLL: Last updated 2025-01-04.
    # ROLL: By https://cr-buildbucket.appspot.com/build/8726637609465866817.
    commit = "32b59456cc056e2be35b7088f84782b9579772a7",
    remote = "https://fuchsia.googlesource.com/fuchsia-infra-bazel-rules",
)

load("@fuchsia_infra//:workspace.bzl", "fuchsia_infra_workspace")

fuchsia_infra_workspace()

FUCHSIA_SDK_VERSION = "version:26.20250102.3.1"

cipd_repository(
    name = "fuchsia_sdk",
    path = "fuchsia/sdk/core/fuchsia-bazel-rules/linux-amd64",
    tag = FUCHSIA_SDK_VERSION,
)

cipd_repository(
    name = "rules_fuchsia",
    path = "fuchsia/development/rules_fuchsia",
    tag = FUCHSIA_SDK_VERSION,
)

register_toolchains("//pw_toolchain/fuchsia:fuchsia_sdk_toolchain")

cipd_repository(
    name = "fuchsia_products_metadata",
    path = "fuchsia/development/product_bundles/v2",
    tag = FUCHSIA_SDK_VERSION,
)

load("//pw_build/bazel_internal/fuchsia_sdk_workspace:products.bzl", "fuchsia_products_repository")

fuchsia_products_repository(
    name = "fuchsia_products",
    metadata_file = "@fuchsia_products_metadata//:product_bundles.json",
)

cipd_repository(
    name = "fuchsia_clang",
    path = "fuchsia/development/fuchsia_clang/linux-amd64",
    tag = "git_revision:aea60ab94db4729bad17daa86ccfc411d48a1699",
)

# TODO: b/354268150 - googletest is in the BCR, but its MODULE.bazel doesn't
# express its dependency on the Fuchsia SDK correctly.
git_repository(
    name = "com_google_googletest",
    commit = "3b6d48e8d5c1d9b3f9f10ac030a94008bfaf032b",
    remote = "https://pigweed.googlesource.com/third_party/github/google/googletest",
)

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

# TODO: https://pwbug.dev/365103864 - Fuzztest is not in the BCR yet (also see
# https://github.com/google/fuzztest/issues/950).
http_archive(
    name = "com_google_fuzztest",
    strip_prefix = "fuzztest-6eb010c7223a6aa609b94d49bfc06ac88f922961",
    url = "https://github.com/google/fuzztest/archive/6eb010c7223a6aa609b94d49bfc06ac88f922961.zip",
)
