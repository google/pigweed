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

http_archive(
    name = "rules_cc",
    sha256 = "2037875b9a4456dce4a79d112a8ae885bbc4aad968e6587dca6e64f3a0900cdf",
    strip_prefix = "rules_cc-0.0.9",
    urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.9/rules_cc-0.0.9.tar.gz"],
)

local_repository(
    name = "pw_toolchain",
    path = "pw_toolchain_bazel",
)

# Setup CIPD client and packages.
# Required by: pigweed.
# Used by modules: all.
cipd_client_repository()

load("//pw_toolchain:register_toolchains.bzl", "register_pigweed_cxx_toolchains")

register_pigweed_cxx_toolchains()

# Set up legacy pw_transfer test binaries.
# Required by: pigweed.
# Used in modules: //pw_transfer.
cipd_repository(
    name = "pw_transfer_test_binaries",
    path = "pigweed/pw_transfer_test_binaries/${os=linux}-${arch=amd64}",
    tag = "version:pw_transfer_test_binaries_528098d588f307881af83f769207b8e6e1b57520-linux-amd64-cipd.cipd",
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

# Used in modules: //pw_grpc
http_archive(
    name = "io_bazel_rules_go",
    sha256 = "7c76d6236b28ff695aa28cf35f95de317a9472fd1fb14ac797c9bf684f09b37c",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
    ],
)

# Used in modules: //pw_grpc
http_archive(
    name = "bazel_gazelle",
    sha256 = "32938bda16e6700063035479063d9d24c60eda8d79fd4739563f50d331cb3209",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
    ],
)

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(version = "1.21.5")

gazelle_dependencies()

load("//pw_grpc:deps.bzl", "pw_grpc_deps")

# gazelle:repository_macro pw_grpc/deps.bzl%pw_grpc_deps
pw_grpc_deps()

http_archive(
    name = "rules_proto",
    sha256 = "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
    strip_prefix = "rules_proto-5.3.0-21.7",
    urls = [
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/5.3.0-21.7.tar.gz",
    ],
)

# Set up Python support.
# Required by: rules_fuzzing, com_github_nanopb_nanopb.
# Used in modules: None.
# TODO: b/310293060 - Switch to an official release when it includes the fix for
# macOS hosts running Python <=3.8.
git_repository(
    name = "rules_python",
    commit = "e06b4bae446706db3414e75d301f56821001b554",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

load("@rules_python//python:repositories.bzl", "py_repositories", "python_register_toolchains")

py_repositories()

# Use Python 3.11 for bazel Python rules.
python_register_toolchains(
    name = "python3",
    # Allows building as root in a docker container. Required by oss-fuzz.
    ignore_root_user_error = True,
    python_version = "3.11",
)

load("@python3//:defs.bzl", "interpreter")
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

# Set up rules for Abseil C++.
# Must be included before com_google_googletest and rules_fuzzing.
# Required by: rules_fuzzing, fuzztest
# Generated by //pw_build/py/pw_build/bazel_to_gn.py
http_archive(
    name = "com_google_absl",
    sha256 = "338420448b140f0dfd1a1ea3c3ce71b3bc172071f24f4d9a57d59b45037da440",
    strip_prefix = "abseil-cpp-20240116.0",
    url = "https://github.com/abseil/abseil-cpp/releases/download/20240116.0/abseil-cpp-20240116.0.tar.gz",
)

# Set up upstream googletest and googlemock.
# Required by: Pigweed.
# Used in modules: //pw_analog, //pw_fuzzer, //pw_i2c.
http_archive(
    name = "com_google_googletest",
    sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7",
    strip_prefix = "googletest-1.14.0",
    urls = [
        "https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz",
    ],
)

# Sets up Bazels documentation generator.
# Required by modules: All
git_repository(
    name = "io_bazel_stardoc",
    commit = "2b801dc9b93f73812948ee4e505805511b0f55dc",
    remote = "https://github.com/bazelbuild/stardoc.git",
)

# Set up Protobuf rules.
# Required by: pigweed.
# Used in modules: //pw_protobuf.
# TODO: pwbug.dev/319717451 - Keep this in sync with the pip requirements.
http_archive(
    name = "com_google_protobuf",
    sha256 = "616bb3536ac1fff3fb1a141450fa28b875e985712170ea7f1bfe5e5fc41e2cd8",
    strip_prefix = "protobuf-24.4",
    url = "https://github.com/protocolbuffers/protobuf/releases/download/v24.4/protobuf-24.4.tar.gz",
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
)

# Registers platforms for use with toolchain resolution
register_execution_platforms("@local_config_platform//:host", "//pw_build/platforms:all")

# Set up rules for fuzz testing.
# Required by: pigweed.
# Used in modules: //pw_protobuf, //pw_tokenizer, //pw_fuzzer.
#
# TODO(b/311746469): Switch back to a released version when possible.
git_repository(
    name = "rules_fuzzing",
    commit = "67ba0264c46c173a75825f2ae0a0b4b9b17c5e59",
    remote = "https://github.com/bazelbuild/rules_fuzzing",
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

# Set up rules for FuzzTest.
# Required by: fuzztest
# Used in modules: //pw_fuzzer.
# Generated by //pw_build/py/pw_build/bazel_to_gn.py
http_archive(
    name = "com_google_fuzztest",
    strip_prefix = "fuzztest-6eb010c7223a6aa609b94d49bfc06ac88f922961",
    url = "https://github.com/google/fuzztest/archive/6eb010c7223a6aa609b94d49bfc06ac88f922961.zip",
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
)

git_repository(
    name = "boringssl",
    commit = "0fd67c76fc4bfb05a665c087ebfead77a3267f6d",
    remote = "https://boringssl.googlesource.com/boringssl",
)

git_repository(
    name = "mbedtls",
    build_file = "//:third_party/mbedtls/BUILD.mbedtls",
    # mbedtls-3.2.1 released 2022-07-12
    commit = "869298bffeea13b205343361b7a7daf2b210e33d",
    remote = "https://pigweed.googlesource.com/third_party/github/ARMmbed/mbedtls",
)

git_repository(
    name = "com_google_emboss",
    commit = "35e21b10019ded9ae14041af9b8e49659d9b327a",
    remote = "https://pigweed.googlesource.com/third_party/github/google/emboss",
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
