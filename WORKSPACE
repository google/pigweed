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
    "cipd_client_repository",
    "cipd_repository",
    "pigweed_deps",
)

# Set up Bazel platforms.
# Required by: pigweed.
# Used in modules: //pw_build, (Assorted modules via select statements).
http_archive(
    name = "platforms",
    sha256 = "8150406605389ececb6da07cbcb509d5637a3ab9a24bc69b1101531367d89d74",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz",
        "https://github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz",
    ],
)

local_repository(
    name = "pw_toolchain",
    path = "pw_toolchain_bazel",
)

# Setup xcode on mac.
load("@pw_toolchain//features/macos:generate_xcode_repository.bzl", "pw_xcode_command_line_tools_repository")

pw_xcode_command_line_tools_repository()

# Setup CIPD client and packages.
# Required by: pigweed.
# Used by modules: all.
pigweed_deps()

load("@cipd_deps//:cipd_init.bzl", "cipd_init")

cipd_init()

cipd_client_repository()

# Set up legacy pw_transfer test binaries.
# Required by: pigweed.
# Used in modules: //pw_transfer.
cipd_repository(
    name = "pw_transfer_test_binaries",
    path = "pigweed/pw_transfer_test_binaries/${os=linux}-${arch=amd64}",
    tag = "version:pw_transfer_test_binaries_528098d588f307881af83f769207b8e6e1b57520-linux-amd64-cipd.cipd",
)

# Fetch llvm toolchain.
# Required by: pigweed.
# Used in modules: //pw_toolchain.
cipd_repository(
    name = "llvm_toolchain",
    path = "fuchsia/third_party/clang/${os}-${arch}",
    tag = "git_revision:8475d0a2b853f6184948b428ec679edf84ed2688",
)

# Fetch linux sysroot for host builds.
# Required by: pigweed.
# Used in modules: //pw_toolchain.
cipd_repository(
    name = "linux_sysroot",
    path = "fuchsia/third_party/sysroot/linux",
    tag = "git_revision:d342388843734b6c5c50fb7e18cd3a76476b93aa",
)

# Note that the order of registration matters: Bazel will use the first
# toolchain compatible with the target platform. So, they should be listed from
# most-restrive to least-restrictive.
register_toolchains(
    "//pw_toolchain/host_clang:host_cc_toolchain_linux_kythe",
    "//pw_toolchain/host_clang:host_cc_toolchain_linux",
    "//pw_toolchain/host_clang:host_cc_toolchain_macos",
)

# Fetch gcc-arm-none-eabi toolchain.
# Required by: pigweed.
# Used in modules: //pw_toolchain.
cipd_repository(
    name = "gcc_arm_none_eabi_toolchain",
    path = "fuchsia/third_party/armgcc/${os}-${arch}",
    tag = "version:2@12.2.mpacbti-rel1.1",
)

register_toolchains(
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m0",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m3",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m4",
    "//pw_toolchain/arm_gcc:arm_gcc_cc_toolchain_cortex-m4+nofp",
)

# Set up Starlark library.
# Required by: io_bazel_rules_go, com_google_protobuf, rules_python
# Used in modules: None.
# This must be instantiated before com_google_protobuf as protobuf_deps() pulls
# in an older version of bazel_skylib. However io_bazel_rules_go requires a
# newer version.
http_archive(
    name = "bazel_skylib",  # 2022-09-01
    sha256 = "4756ab3ec46d94d99e5ed685d2d24aece484015e45af303eb3a11cab3cdc2e71",
    strip_prefix = "bazel-skylib-1.3.0",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/refs/tags/1.3.0.zip"],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

# Set up Python support.
# Required by: rules_fuzzing, com_github_nanopb_nanopb.
# Used in modules: None.
http_archive(
    name = "rules_python",
    sha256 = "0a8003b044294d7840ac7d9d73eef05d6ceb682d7516781a4ec62eeb34702578",
    strip_prefix = "rules_python-0.24.0",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.24.0/rules_python-0.24.0.tar.gz",
)

load("@rules_python//python:repositories.bzl", "python_register_toolchains")

# Use Python 3.10 for bazel Python rules.
python_register_toolchains(
    name = "python3_10",
    # Allows building as root in a docker container. Required by oss-fuzz.
    ignore_root_user_error = True,
    python_version = "3.10",
)

load("@python3_10//:defs.bzl", "interpreter")
load("@rules_python//python:pip.bzl", "pip_parse")

# Specify third party Python package versions with pip_parse.
# pip_parse will generate and expose a repository for each package in the
# requirements_lock file named @python_packages_{PACKAGE}.
pip_parse(
    name = "python_packages",
    python_interpreter_target = interpreter,
    requirements_darwin = "//pw_env_setup/py/pw_env_setup/virtualenv_setup:upstream_requirements_darwin_lock.txt",
    requirements_linux = "//pw_env_setup/py/pw_env_setup/virtualenv_setup:upstream_requirements_linux_lock.txt",
    requirements_windows = "//pw_env_setup/py/pw_env_setup/virtualenv_setup:upstream_requirements_windows_lock.txt",
)

load("@python_packages//:requirements.bzl", "install_deps")

# Run pip install for all @python_packages_*//:pkg deps.
install_deps()

# Set up upstream googletest and googlemock.
# Required by: Pigweed.
# Used in modules: //pw_analog, //pw_fuzzer, //pw_i2c.
http_archive(
    name = "com_google_googletest",
    sha256 = "ad7fdba11ea011c1d925b3289cf4af2c66a352e18d4c7264392fead75e919363",
    strip_prefix = "googletest-1.13.0",
    urls = [
        "https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz",
    ],
)

# Sets up Bazels documentation generator.
# Required by modules: All
git_repository(
    name = "io_bazel_stardoc",
    commit = "2b801dc9b93f73812948ee4e505805511b0f55dc",
    remote = "https://github.com/bazelbuild/stardoc.git",
    shallow_since = "1651081130 -0400",
)

# Set up Protobuf rules.
# Required by: pigweed.
# Used in modules: //pw_protobuf.
http_archive(
    name = "com_google_protobuf",
    sha256 = "c6003e1d2e7fefa78a3039f19f383b4f3a61e81be8c19356f85b6461998ad3db",
    strip_prefix = "protobuf-3.17.3",
    url = "https://github.com/protocolbuffers/protobuf/archive/v3.17.3.tar.gz",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# Setup Nanopb protoc plugin.
# Required by: Pigweed.
# Used in modules: pw_protobuf.
http_archive(
    name = "com_github_nanopb_nanopb",
    sha256 = "3f78bf63722a810edb6da5ab5f0e76c7db13a961c2aad4ab49296e3095d0d830",
    strip_prefix = "nanopb-0.4.8",
    url = "https://github.com/nanopb/nanopb/archive/refs/tags/0.4.8.tar.gz",
)

load("@com_github_nanopb_nanopb//extra/bazel:nanopb_deps.bzl", "nanopb_deps")

nanopb_deps()

load("@com_github_nanopb_nanopb//extra/bazel:python_deps.bzl", "nanopb_python_deps")

nanopb_python_deps(interpreter)

load("@com_github_nanopb_nanopb//extra/bazel:nanopb_workspace.bzl", "nanopb_workspace")

nanopb_workspace()

# Rust Support
#

http_archive(
    name = "rules_rust",
    patch_args = ["-p1"],
    patches = [
        # Fix rustdoc test w/ proc macros
        # https://github.com/bazelbuild/rules_rust/pull/1952
        "//pw_rust/bazel_patches:0001-rustdoc_test-Apply-prefix-stripping-to-proc_macro-de.patch",
        # Adds prototype functionality for documenting multiple crates in one
        # HTML output directory.  While the approach in this patch may have
        # issues scaling to giant mono-repos, it is apporpriate for embedded
        # projects and minimally invasive and should be easy to maintain.  Once
        # the `rules_rust` community decides on a way to propperly support this,
        # we will migrate to that solution.
        # https://github.com/konkers/rules_rust/tree/wip/rustdoc
        "//pw_rust/bazel_patches:0002-PROTOTYPE-Add-ability-to-document-multiple-crates-at.patch",
    ],
    sha256 = "9d04e658878d23f4b00163a72da3db03ddb451273eb347df7d7c50838d698f49",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/rules_rust-v0.26.0.tar.gz"],
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies")

rules_rust_dependencies()

load(
    "//pw_toolchain/rust:defs.bzl",
    "pw_rust_register_toolchain_and_target_repos",
    "pw_rust_register_toolchains",
)

pw_rust_register_toolchain_and_target_repos(
    cipd_tag = "rust_revision:faee636ebfff793ea9dcff17960a611b580e3cd5",
)

# Allows creation of a `rust-project.json` file to allow rust analyzer to work.
load("@rules_rust//tools/rust_analyzer:deps.bzl", "rust_analyzer_dependencies")

rust_analyzer_dependencies()

pw_rust_register_toolchains()

# Vendored third party rust crates.
git_repository(
    name = "rust_crates",
    commit = "6d975531f7672cc6aa54bdd7517e1beeffa578da",
    remote = "https://pigweed.googlesource.com/third_party/rust_crates",
    shallow_since = "1675359057 +0000",
)

# Registers platforms for use with toolchain resolution
register_execution_platforms("@local_config_platform//:host", "//pw_build/platforms:all")

# Required by: rules_fuzzing, fuzztest
#
# Provided here explicitly to override an old version of absl that
# rules_fuzzing_dependencies attempts to pull in. That version has
# many compiler warnings on newer clang versions.
http_archive(
    name = "com_google_absl",
    sha256 = "3ea49a7d97421b88a8c48a0de16c16048e17725c7ec0f1d3ea2683a2a75adc21",
    strip_prefix = "abseil-cpp-20230125.0",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20230125.0.tar.gz"],
)

# Set up rules for fuzz testing.
# Required by: pigweed.
# Used in modules: //pw_protobuf, //pw_tokenizer, //pw_fuzzer.
http_archive(
    name = "rules_fuzzing",
    sha256 = "d9002dd3cd6437017f08593124fdd1b13b3473c7b929ceb0e60d317cb9346118",
    strip_prefix = "rules_fuzzing-0.3.2",
    urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.3.2.zip"],
)

load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")

rules_fuzzing_dependencies()

load("@rules_fuzzing//fuzzing:init.bzl", "rules_fuzzing_init")

rules_fuzzing_init()

load("@fuzzing_py_deps//:requirements.bzl", fuzzing_install_deps = "install_deps")

fuzzing_install_deps()

# Required by: fuzztest
http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "f89c61410a072e5cbcf8c27e3a778da7d6fd2f2b5b1445cd4f4508bee946ab0f",
    strip_prefix = "re2-2022-06-01",
    url = "https://github.com/google/re2/archive/refs/tags/2022-06-01.tar.gz",
)

# Required by: pigweed.
# Used in modules: //pw_fuzzer.
FUZZTEST_COMMIT = "f2e9e2a19a7b16101d1e6f01a87e639687517a1c"

http_archive(
    name = "com_google_fuzztest",
    strip_prefix = "fuzztest-" + FUZZTEST_COMMIT,
    url = "https://github.com/google/fuzztest/archive/" + FUZZTEST_COMMIT + ".zip",
)

RULES_JVM_EXTERNAL_TAG = "2.8"

RULES_JVM_EXTERNAL_SHA = "79c9850690d7614ecdb72d68394f994fef7534b292c4867ce5e7dec0aa7bdfad"

http_archive(
    name = "rules_jvm_external",
    sha256 = RULES_JVM_EXTERNAL_SHA,
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:defs.bzl", "maven_install")

# Pull in packages for the pw_rpc Java client with Maven.
maven_install(
    artifacts = [
        "com.google.auto.value:auto-value:1.8.2",
        "com.google.auto.value:auto-value-annotations:1.8.2",
        "com.google.code.findbugs:jsr305:3.0.2",
        "com.google.flogger:flogger:0.7.1",
        "com.google.flogger:flogger-system-backend:0.7.1",
        "com.google.guava:guava:31.0.1-jre",
        "com.google.truth:truth:1.1.3",
        "org.mockito:mockito-core:4.1.0",
    ],
    repositories = [
        "https://maven.google.com/",
        "https://jcenter.bintray.com/",
        "https://repo1.maven.org/maven2",
    ],
)

new_git_repository(
    name = "micro_ecc",
    build_file = "//:third_party/micro_ecc/BUILD.micro_ecc",
    commit = "b335ee812bfcca4cd3fb0e2a436aab39553a555a",
    remote = "https://github.com/kmackay/micro-ecc.git",
    shallow_since = "1648504566 -0700",
)

git_repository(
    name = "boringssl",
    commit = "0fd67c76fc4bfb05a665c087ebfead77a3267f6d",
    remote = "https://boringssl.googlesource.com/boringssl",
    shallow_since = "1637714942 +0000",
)

git_repository(
    name = "mbedtls",
    build_file = "//:third_party/mbedtls/BUILD.mbedtls",
    # mbedtls-3.2.1 released 2022-07-12
    commit = "869298bffeea13b205343361b7a7daf2b210e33d",
    remote = "https://pigweed.googlesource.com/third_party/github/ARMmbed/mbedtls",
    shallow_since = "1648504566 -0700",
)

http_archive(
    name = "freertos",
    build_file = "//:third_party/freertos/BUILD.bazel",
    sha256 = "89af32b7568c504624f712c21fe97f7311c55fccb7ae6163cda7adde1cde7f62",
    strip_prefix = "FreeRTOS-Kernel-10.5.1",
    urls = ["https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V10.5.1.tar.gz"],
)

http_archive(
    name = "stm32f4xx_hal_driver",
    build_file = "//third_party/stm32cube:stm32_hal_driver.BUILD.bazel",
    sha256 = "c8741e184555abcd153f7bdddc65e4b0103b51470d39ee0056ce2f8296b4e835",
    strip_prefix = "stm32f4xx_hal_driver-1.8.0",
    urls = ["https://github.com/STMicroelectronics/stm32f4xx_hal_driver/archive/refs/tags/v1.8.0.tar.gz"],
)

http_archive(
    name = "cmsis_device_f4",
    build_file = "//third_party/stm32cube:cmsis_device.BUILD.bazel",
    sha256 = "6390baf3ea44aff09d0327a3c112c6ca44418806bfdfe1c5c2803941c391fdce",
    strip_prefix = "cmsis_device_f4-2.6.8",
    urls = ["https://github.com/STMicroelectronics/cmsis_device_f4/archive/refs/tags/v2.6.8.tar.gz"],
)

http_archive(
    name = "cmsis_core",
    build_file = "//third_party/stm32cube:cmsis_core.BUILD.bazel",
    sha256 = "f711074a546bce04426c35e681446d69bc177435cd8f2f1395a52db64f52d100",
    strip_prefix = "cmsis_core-5.4.0_cm4",
    urls = ["https://github.com/STMicroelectronics/cmsis_core/archive/refs/tags/v5.4.0_cm4.tar.gz"],
)
