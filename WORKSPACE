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
    # ROLL: Last updated 2024-08-04.
    # ROLL: By https://cr-buildbucket.appspot.com/build/8740567680888136625.
    commit = "6833492d54bfa587f97f1a2ec2636c1013596782",
    remote = "https://fuchsia.googlesource.com/fuchsia-infra-bazel-rules",
)

load("@fuchsia_infra//:workspace.bzl", "fuchsia_infra_workspace")

fuchsia_infra_workspace()

FUCHSIA_LINUX_SDK_VERSION = "version:22.20240717.3.1"

# The Fuchsia SDK is no longer released for MacOS, so we need to pin an older
# version, from the halcyon days when this OS was still supported.
FUCHSIA_MAC_SDK_VERSION = "version:20.20240408.3.1"

cipd_repository(
    name = "fuchsia_sdk",
    path = "fuchsia/sdk/core/fuchsia-bazel-rules/${os}-amd64",
    tag_by_os = {
        "linux": FUCHSIA_LINUX_SDK_VERSION,
        "mac": FUCHSIA_MAC_SDK_VERSION,
    },
)

load("@fuchsia_sdk//fuchsia:deps.bzl", "rules_fuchsia_deps")

rules_fuchsia_deps()

register_toolchains("@fuchsia_sdk//:fuchsia_toolchain_sdk")

cipd_repository(
    name = "fuchsia_products_metadata",
    path = "fuchsia/development/product_bundles/v2",
    tag_by_os = {
        "linux": FUCHSIA_LINUX_SDK_VERSION,
        "mac": FUCHSIA_MAC_SDK_VERSION,
    },
)

load("@fuchsia_sdk//fuchsia:products.bzl", "fuchsia_products_repository")

fuchsia_products_repository(
    name = "fuchsia_products",
    metadata_file = "@fuchsia_products_metadata//:product_bundles.json",
)

load("@fuchsia_sdk//fuchsia:clang.bzl", "fuchsia_clang_repository")

fuchsia_clang_repository(
    name = "fuchsia_clang",
    # TODO: https://pwbug.dev/346354914 - Reuse @llvm_toolchain. This currently
    # leads to flaky loading phase errors!
    # from_workspace = "@llvm_toolchain//:BUILD",
    cipd_tag = "git_revision:c58bc24fcf678c55b0bf522be89eff070507a005",
    sdk_root_label = "@fuchsia_sdk",
)

load("@fuchsia_clang//:defs.bzl", "register_clang_toolchains")

register_clang_toolchains()

# Since Fuchsia doesn't release arm64 SDKs, use this to gate Fuchsia targets.
load("//pw_env_setup:bazel/host_metadata_repository.bzl", "host_metadata_repository")

host_metadata_repository(
    name = "host_metadata",
)

# TODO: b/354268150 - googletest is in the BCR, but its MODULE.bazel doesn't
# express its dependency on the Fuchsia SDK correctly.
git_repository(
    name = "com_google_googletest",
    commit = "3b6d48e8d5c1d9b3f9f10ac030a94008bfaf032b",
    remote = "https://pigweed.googlesource.com/third_party/github/google/googletest",
)

load(
    "//pw_toolchain/rust:defs.bzl",
    "pw_rust_register_toolchain_and_target_repos",
    "pw_rust_register_toolchains",
)

pw_rust_register_toolchain_and_target_repos(
    cipd_tag = "rust_revision:bf9c7a64ad222b85397573668b39e6d1ab9f4a72",
)

# Allows creation of a `rust-project.json` file to allow rust analyzer to work.
load("@rules_rust//tools/rust_analyzer:deps.bzl", "rust_analyzer_dependencies")

rust_analyzer_dependencies()

pw_rust_register_toolchains()

# Vendored third party rust crates.
git_repository(
    name = "rust_crates",
    commit = "de54de1a2683212d8edb4e15ec7393eb013c849c",
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
git_repository(
    name = "mbedtls",
    build_file = "//:third_party/mbedtls/mbedtls.BUILD.bazel",
    # mbedtls-3.2.1 released 2022-07-12
    commit = "869298bffeea13b205343361b7a7daf2b210e33d",
    remote = "https://pigweed.googlesource.com/third_party/github/ARMmbed/mbedtls",
)

# TODO: https://pwbug.dev/354747966 - Update the BCR version of Emboss.
git_repository(
    name = "com_google_emboss",
    remote = "https://pigweed.googlesource.com/third_party/github/google/emboss",
    # Also update emboss tag in pw_package/py/pw_package/packages/emboss.py
    tag = "v2024.0718.173957",
)

git_repository(
    name = "icu",
    build_file = "//third_party/icu:icu.BUILD.bazel",
    commit = "ef02cc27c0faceffc9345e11a35769ae92b836fb",
    remote = "https://fuchsia.googlesource.com/third_party/icu",
)
