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

"""Dependency registration helpers for users of Pigweed.

Before loading this bzl file, you must first:

* Declare the Pigweed repository.
* Declare the pw_toolchain repository.
* Initialize the CIPD client repository.
"""

load("//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl", "cipd_repository")
load("//pw_toolchain:xcode.bzl", "xcode_sdk_repository")

_ALL_TOOLCHAINS = [
    "//pw_toolchain:cc_toolchain_cortex-m0",
    "//pw_toolchain:cc_toolchain_cortex-m0plus",
    "//pw_toolchain:cc_toolchain_cortex-m33",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m3",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m4",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m7",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m33",
    "//pw_toolchain/host_clang:host_cc_toolchain_linux",
    "//pw_toolchain/host_clang:host_cc_toolchain_macos",
]

# buildifier: disable=unnamed-macro
def register_pigweed_cxx_toolchains(
        toolchains = _ALL_TOOLCHAINS,
        clang_tag = None,
        arm_gcc_tag = None):
    """This function registers Pigweed's upstream toolchains.

    WARNING: The configurations and permutations of these toolchains are subject
    to change without notice. These changes may cause your build to break, so if
    you want to minimize this churn, consider declaring your own toolchains to
    have more control over the stability.

    Args:
        toolchains: Toolchains from Pigweed to register after declaring the
            prerequisite repositories.
        clang_tag: The CIPD version tag to use when fetching clang.
        arm_gcc_tag: The CIPD version tag to use when fetching
            arm-none-eabi-gcc.
    """

    # Generate xcode repository on macOS.
    xcode_sdk_repository(
        name = "macos_sysroot",
        build_file = "@pigweed//pw_toolchain/host_clang:macos_sysroot.BUILD",
    )

    # Fetch llvm toolchain for device.
    cipd_repository(
        name = "llvm_toolchain_device",
        build_file = "@pigweed//pw_toolchain/build_external:llvm_clang.BUILD",
        path = "fuchsia/third_party/clang/${os}-${arch}",
        tag = "git_revision:84af3ee5124de3385b829c3a9980fd734f0d92e8" if not clang_tag else clang_tag,
    )

    # Fetch llvm toolchain for host.
    cipd_repository(
        name = "llvm_toolchain",
        build_file = "@pigweed//pw_toolchain/build_external:llvm_clang.BUILD",
        path = "fuchsia/third_party/clang/${os}-${arch}",
        tag = "git_revision:84af3ee5124de3385b829c3a9980fd734f0d92e8" if not clang_tag else clang_tag,
    )

    # Fetch linux sysroot for host builds.
    cipd_repository(
        name = "linux_sysroot",
        build_file = "@pigweed//pw_toolchain/host_clang:linux_sysroot.BUILD",
        path = "fuchsia/third_party/sysroot/bionic",
        tag = "git_revision:702eb9654703a7cec1cadf93a7e3aa269d053943",
    )

    # Fetch gcc-arm-none-eabi toolchain.
    cipd_repository(
        name = "gcc_arm_none_eabi_toolchain",
        build_file = "@pigweed//pw_toolchain/build_external:arm_none_eabi_gcc.BUILD",
        path = "fuchsia/third_party/armgcc/${os}-${arch}",
        tag = "version:2@12.2.MPACBTI-Rel1.1" if not arm_gcc_tag else arm_gcc_tag,
    )

    # TODO: https://pwbug.dev/346388161 - Temporary migration shim.
    cipd_repository(
        name = "legacy_gcc_arm_none_eabi_toolchain",
        build_file = "@pw_toolchain//build_external:gcc_arm_none_eabi.BUILD",
        path = "fuchsia/third_party/armgcc/${os}-${arch}",
        tag = "version:2@12.2.MPACBTI-Rel1.1" if not arm_gcc_tag else arm_gcc_tag,
    )

    native.register_toolchains(*toolchains)
