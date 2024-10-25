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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(
    "//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl",
    "cipd_repository",
)

# Set up legacy pw_transfer test binaries.
# Required by: pigweed.
# Used in modules: //pw_transfer.
cipd_repository(
    name = "pw_transfer_test_binaries",
    path = "pigweed/pw_transfer_test_binaries/${os=linux}-${arch=amd64}",
    tag = "version:pw_transfer_test_binaries_528098d588f307881af83f769207b8e6e1b57520-linux-amd64-cipd.cipd",
)

# Set up bloaty size profiler.
# Required by: pigweed.
# Used in modules: //pw_bloat.
cipd_repository(
    name = "bloaty",
    path = "fuchsia/third_party/bloaty/${os}-amd64",
    tag = "git_revision:c057ba4f43db0506d4ba8c096925b054b02a8bd3",
)

# Setup Fuchsia SDK.
# Required by: bt-host.
# Used in modules: //pw_bluetooth_sapphire.
# NOTE: These blocks cannot feasibly be moved into a macro.
# See https://github.com/bazelbuild/bazel/issues/1550
git_repository(
    name = "fuchsia_infra",
    # ROLL: Warning: this entry is automatically updated.
    # ROLL: Last updated 2024-10-19.
    # ROLL: By https://cr-buildbucket.appspot.com/build/8733613569699455441.
    commit = "8c99ec2804c67b6888a3de043bae1ac69e9b4a26",
    remote = "https://fuchsia.googlesource.com/fuchsia-infra-bazel-rules",
)

load("@fuchsia_infra//:workspace.bzl", "fuchsia_infra_workspace")

fuchsia_infra_workspace()

FUCHSIA_SDK_VERSION = "version:25.20241025.4.1"

cipd_repository(
    name = "fuchsia_sdk",
    path = "fuchsia/sdk/core/fuchsia-bazel-rules/linux-amd64",
    tag = FUCHSIA_SDK_VERSION,
)

register_toolchains("@fuchsia_sdk//:fuchsia_toolchain_sdk")

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

# Vendored third party rust crates.
git_repository(
    name = "rust_crates",
    commit = "ed1ec1bd240b9446b30af5331b960871a0503a6c",
    remote = "https://pigweed.googlesource.com/third_party/rust_crates",
)

# Registers platforms for use with toolchain resolution
register_execution_platforms("@local_config_platform//:host", "//pw_build/platforms:all")

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

# Fuzztest is not in the BCR yet (https://github.com/google/fuzztest/issues/950).
http_archive(
    name = "com_google_fuzztest",
    strip_prefix = "fuzztest-6eb010c7223a6aa609b94d49bfc06ac88f922961",
    url = "https://github.com/google/fuzztest/archive/6eb010c7223a6aa609b94d49bfc06ac88f922961.zip",
)

new_git_repository(
    name = "micro_ecc",
    build_file = "//:third_party/micro_ecc/BUILD.micro_ecc",
    commit = "b335ee812bfcca4cd3fb0e2a436aab39553a555a",
    remote = "https://github.com/kmackay/micro-ecc.git",
)

# TODO: https://pwbug.dev/354749299 - Use the BCR version of mbedtls.
http_archive(
    name = "mbedtls",
    build_file = "//:third_party/mbedtls/mbedtls.BUILD.bazel",
    sha256 = "241c68402cef653e586be3ce28d57da24598eb0df13fcdea9d99bfce58717132",
    strip_prefix = "mbedtls-2.28.8",
    url = "https://github.com/Mbed-TLS/mbedtls/releases/download/v2.28.8/mbedtls-2.28.8.tar.bz2",
)

# TODO: https://pwbug.dev/354747966 - Update the BCR version of Emboss.
git_repository(
    name = "com_google_emboss",
    # LINT.IfChange(emboss)
    remote = "https://pigweed.googlesource.com/third_party/github/google/emboss",
    tag = "v2024.0809.170004",
    # LINT.ThenChange(/pw_package/py/pw_package/packages/emboss.py:emboss)
)
