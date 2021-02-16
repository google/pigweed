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
    managed_directories = {"@npm": ["node_modules"]},
)

# Set up build_bazel_rules_nodejs.
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "4952ef879704ab4ad6729a29007e7094aef213ea79e9f2e94cbe1c9a753e63ef",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/2.2.0/rules_nodejs-2.2.0.tar.gz"],
)

# Get the latest LTS version of Node.
load(
    "@build_bazel_rules_nodejs//:index.bzl",
    "node_repositories",
    "yarn_install",
)

node_repositories(package_json = ["//:package.json"])

yarn_install(
    name = "npm",
    package_json = "//:package.json",
    yarn_lock = "//:yarn.lock",
)

# Set up Karma.
load("@npm//@bazel/karma:package.bzl", "npm_bazel_karma_dependencies")

npm_bazel_karma_dependencies()

load(
    "@io_bazel_rules_webtesting//web:repositories.bzl",
    "web_test_repositories",
)

web_test_repositories()

load(
    "@io_bazel_rules_webtesting//web/versioned:browsers-0.3.2.bzl",
    "browser_repositories",
)

browser_repositories(
    chromium = True,
    firefox = True,
)

# Setup embedded C/C++ toolchains.
git_repository(
    name = "bazel_embedded",
    commit = "89c05fa415218abd2e24fa7016cb7903317d606b",
    remote = "https://github.com/silvergasp/bazel-embedded.git",
)

# Instantiate Pigweed configuration for embedded toolchain,
# this must be called before bazel_embedded_deps.
load(
    "//pw_build:pigweed_toolchain_upstream.bzl",
    "toolchain_upstream_deps",
)

toolchain_upstream_deps()

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

# Fetch LLVM/Clang compiler and register for toolchain resolution.
load(
    "@bazel_embedded//toolchains/compilers/llvm:llvm_repository.bzl",
    "llvm_repository",
)

llvm_repository(
    name = "com_llvm_compiler",
)

load(
    "@bazel_embedded//toolchains/clang:clang_toolchain.bzl",
    "register_clang_toolchain",
)

register_clang_toolchain()

register_execution_platforms("//pw_build/platforms:all")
