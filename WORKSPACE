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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
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
    sha256 = "5308fc1d8865406a49427ba24a9ab53087f17f5266a7aabbfc28823f3916e1ca",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
        "https://github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
    ],
)

# Setup CIPD client and packages.
# Required by: pigweed.
# Used by modules: all.
pigweed_deps()

load("@cipd_deps//:cipd_init.bzl", "cipd_init")

cipd_init()

cipd_client_repository()

cipd_repository(
    name = "pw_transfer_test_binaries",
    path = "pigweed/pw_transfer_test_binaries/${os=linux}-${arch=amd64}",
    tag = "version:pw_transfer_test_binaries_528098d588f307881af83f769207b8e6e1b57520-linux-amd64-cipd.cipd",
)

# Set up Python support.
# Required by: rules_fuzzing, com_github_nanopb_nanopb.
# Used in modules: None.
http_archive(
    name = "rules_python",
    sha256 = "9fcf91dbcc31fde6d1edb15f117246d912c33c36f44cf681976bd886538deba6",
    strip_prefix = "rules_python-0.8.0",
    url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/0.8.0.tar.gz",
)

load("@rules_python//python:repositories.bzl", "python_register_toolchains")

# Use Python 3.10 for bazel Python rules.
python_register_toolchains(
    name = "python3_10",
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
    requirements_lock = "//pw_env_setup/py/pw_env_setup/virtualenv_setup:constraint.list",
)

load("@python_packages//:requirements.bzl", "install_deps")

# Run pip install for all @python_packages_*//:pkg deps.
install_deps()

# Set up Starlark library.
# Required by: io_bazel_rules_go, com_google_protobuf.
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

# Set up upstream googletest and googlemock.
# Required by: Pigweed.
# Used in modules: //pw_analog, //pw_i2c.
http_archive(
    name = "com_google_googletest",
    sha256 = "ad7fdba11ea011c1d925b3289cf4af2c66a352e18d4c7264392fead75e919363",
    strip_prefix = "googletest-1.13.0",
    urls = [
        "https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz",
    ],
)

# Set up host hermetic host toolchain.
# Required by: All cc targets.
# Used in modules: All cc targets.
git_repository(
    name = "rules_cc_toolchain",
    commit = "9f209fda87414285bc66accd3612575b29760fba",
    remote = "https://github.com/bazelembedded/rules_cc_toolchain",
    shallow_since = "1675385535 -0800",
)

load("@rules_cc_toolchain//:rules_cc_toolchain_deps.bzl", "rules_cc_toolchain_deps")

rules_cc_toolchain_deps()

load("@rules_cc_toolchain//cc_toolchain:cc_toolchain.bzl", "register_cc_toolchains")

register_cc_toolchains()

# Sets up Bazels documentation generator.
# Required by: rules_cc_toolchain.
# Required by modules: All
git_repository(
    name = "io_bazel_stardoc",
    commit = "2b801dc9b93f73812948ee4e505805511b0f55dc",
    remote = "https://github.com/bazelbuild/stardoc.git",
    shallow_since = "1651081130 -0400",
)

# Set up tools to build custom GRPC rules.
#
# We use a fork that silences some zlib compilation warnings.
#
# Required by: pigweed.
# Used in modules: //pw_protobuf.
git_repository(
    name = "rules_proto_grpc",
    commit = "2fbf774a5553b773372f7b91f9b1dc06ee0da2d3",
    remote = "https://github.com/tpudlik/rules_proto_grpc.git",
    shallow_since = "1675375991 -0800",
)

load(
    "@rules_proto_grpc//:repositories.bzl",
    "rules_proto_grpc_repos",
    "rules_proto_grpc_toolchains",
)

rules_proto_grpc_toolchains()

rules_proto_grpc_repos()

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
git_repository(
    name = "com_github_nanopb_nanopb",
    commit = "ee27d70d329e1718f39eea1f425178e747263173",
    remote = "https://github.com/nanopb/nanopb.git",
    shallow_since = "1641373017 +0800",
)

load("@com_github_nanopb_nanopb//extra/bazel:nanopb_deps.bzl", "nanopb_deps")

nanopb_deps()

load("@com_github_nanopb_nanopb//extra/bazel:python_deps.bzl", "nanopb_python_deps")

nanopb_python_deps(interpreter)

load("@com_github_nanopb_nanopb//extra/bazel:nanopb_workspace.bzl", "nanopb_workspace")

nanopb_workspace()

# Set up embedded C/C++ toolchains.
# Required by: pigweed.
# Used in modules: //pw_polyfill, //pw_build (all pw_cc* targets).
git_repository(
    name = "bazel_embedded",
    commit = "91dcc13ebe5df755ca2fe896ff6f7884a971d05b",
    remote = "https://github.com/bazelembedded/bazel-embedded.git",
    shallow_since = "1631751909 +0800",
)

# Configure bazel_embedded toolchains and platforms.
load(
    "@bazel_embedded//:bazel_embedded_deps.bzl",
    "bazel_embedded_deps",
)

bazel_embedded_deps()

load(
    "@bazel_embedded//platforms:execution_platforms.bzl",
    "register_platforms",
)

register_platforms()

# Fetch gcc-arm-none-eabi compiler and register for toolchain
# resolution.
load(
    "@bazel_embedded//toolchains/compilers/gcc_arm_none_eabi:gcc_arm_none_repository.bzl",
    "gcc_arm_none_compiler",
)

gcc_arm_none_compiler()

load(
    "@bazel_embedded//toolchains/gcc_arm_none_eabi:gcc_arm_none_toolchain.bzl",
    "register_gcc_arm_none_toolchain",
)

register_gcc_arm_none_toolchain()

# Rust Support
#

git_repository(
    name = "rules_rust",
    # Pulls in the main branch with https://github.com/bazelbuild/rules_rust/pull/1803
    # merged.  Once a release is cut with that commit, we should switch to
    # using a release tarbal.
    commit = "a5853fd37053b65ee30ba4f8064b9db67c90d53f",
    remote = "https://github.com/bazelbuild/rules_rust",
    shallow_since = "1675302817 -0800",
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_analyzer_toolchain_repository", "rust_repository_set")

rules_rust_dependencies()

# Here we pull in a specific toolchain.  Unfortunately `rust_repository_set`
# does not provide a way to add `target_compatible_with` options which are
# needed to be compatible with `@bazel_embedded` (specifically
# `@bazel_embedded//constraints/fpu:none` which is specified in
# `//platforms`)
#
# See `//toolchain:rust_linux_x86_64` for how this is used.
#
# Note: This statement creates name mangled remotes of the form:
# `@{name}__{triplet}_tools`
# (example: `@rust_linux_x86_64__thumbv7m-none-eabi_tools/`)
rust_repository_set(
    name = "rust_linux_x86_64",
    edition = "2021",
    exec_triple = "x86_64-unknown-linux-gnu",
    extra_target_triples = [
        "thumbv7m-none-eabi",
        "thumbv6m-none-eabi",
    ],
    versions = ["1.67.0"],
)

# Registers our Rust toolchains that are compatable with `@bazel_embedded`.
register_toolchains(
    "//pw_toolchain:thumbv7m_rust_linux_x86_64",
    "//pw_toolchain:thumbv6m_rust_linux_x86_64",
)

# Allows creation of a `rust-project.json` file to allow rust analyzer to work.
load("@rules_rust//tools/rust_analyzer:deps.bzl", "rust_analyzer_dependencies")

# Since we do not use rust_register_toolchains, we need to define a
# rust_analyzer_toolchain.
register_toolchains(rust_analyzer_toolchain_repository(
    name = "rust_analyzer_toolchain",
    # This should match the currently registered toolchain.
    version = "1.67.0",
))

rust_analyzer_dependencies()

# Vendored third party rust crates.
git_repository(
    name = "rust_crates",
    commit = "c39c1d1d4e4bdf2d8145beb8882af6f6e4e6dbbc",
    remote = "https://pigweed.googlesource.com/third_party/rust_crates",
    shallow_since = "1675359057 +0000",
)

# Registers platforms for use with toolchain resolution
register_execution_platforms("//pw_build/platforms:all")

load("//pw_build:target_config.bzl", "pigweed_config")

# Configure Pigweeds backend.
pigweed_config(
    name = "pigweed_config",
    build_file = "//targets:default_config.BUILD",
)

# Required by: rules_fuzzing.
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

http_archive(
    name = "freertos",
    build_file = "//:third_party/freertos/BUILD.bazel",
    sha256 = "89af32b7568c504624f712c21fe97f7311c55fccb7ae6163cda7adde1cde7f62",
    strip_prefix = "FreeRTOS-Kernel-10.5.1",
    urls = ["https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V10.5.1.tar.gz"],
)
