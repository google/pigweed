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

load(
    "@pw_toolchain//features/macos:generate_xcode_repository.bzl",
    "pw_xcode_command_line_tools_repository",
)
load(
    "//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl",
    "cipd_repository",
)

# buildifier: disable=unnamed-macro
def register_pigweed_cxx_toolchains():
    """This function registers Pigweed's upstream toolchains.

    WARNING: The configurations and permutations of these toolchains are subject
    to change without notice. These changes may cause your build to break, so if
    you want to minimize this churn, consider declaring your own toolchains to
    have more control over the stability.
    """

    # Generate xcode repository on macOS.
    pw_xcode_command_line_tools_repository()

    # Fetch llvm toolchain.
    cipd_repository(
        name = "llvm_toolchain",
        build_file = "@pw_toolchain//build_external:llvm_clang.BUILD",
        path = "fuchsia/third_party/clang/${os}-${arch}",
        tag = "git_revision:c58bc24fcf678c55b0bf522be89eff070507a005",
    )

    # Fetch linux sysroot for host builds.
    cipd_repository(
        name = "linux_sysroot",
        path = "fuchsia/third_party/sysroot/linux",
        tag = "git_revision:d342388843734b6c5c50fb7e18cd3a76476b93aa",
    )

    # Fetch gcc-arm-none-eabi toolchain.
    cipd_repository(
        name = "gcc_arm_none_eabi_toolchain",
        build_file = "@pw_toolchain//build_external:gcc_arm_none_eabi.BUILD",
        path = "fuchsia/third_party/armgcc/${os}-${arch}",
        tag = "version:2@12.2.MPACBTI-Rel1.1",
    )

    native.register_toolchains(
        "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m0",
        "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m3",
        "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m4",
        "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m4+nofp",
        "//pw_toolchain/host_clang:host_cc_toolchain_linux",
        "//pw_toolchain/host_clang:host_cc_toolchain_macos",
    )
