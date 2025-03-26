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
"""Utilities for declaring Rust toolchains that are compatible with Arm gcc."""

load("@rules_rust//rust:toolchain.bzl", "rust_analyzer_toolchain", "rust_toolchain")
load("//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl", "cipd_repository")
load(":toolchains.bzl", "CHANNELS", "EXTRA_TARGETS", "HOSTS")

_rust_toolchain_repo_template = """\
load("{pigweed_repo_name}//pw_toolchain/rust:defs.bzl", "pw_rust_declare_toolchain_targets")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

pw_rust_declare_toolchain_targets()
"""

def _rust_toolchain_repo_impl(ctx):
    ctx.file("BUILD", _rust_toolchain_repo_template.format(pigweed_repo_name = ctx.attr.pigweed_repo_name))
    pass

_rust_toolchain_repo = repository_rule(
    _rust_toolchain_repo_impl,
    attrs = {
        "pigweed_repo_name": attr.string(
            doc = "The name of the pigweed used to reference build files for the registered repositories.",
        ),
    },
)

# buildifier: disable=unnamed-macro
def pw_rust_register_toolchain_and_target_repos(cipd_tag, pigweed_repo_name = "@pigweed"):
    """Declare and register CIPD repos for Rust toolchain and target rupport.

    Args:
      cipd_tag: Tag with which to select specific package versions.
      pigweed_repo_name: The name of the pigweed used to reference build files
        for the registered repositories.  Defaults to "@pigweed".
    """
    toolchain_repo_name = "{}_rust_toolchain_repo".format(pigweed_repo_name).lstrip("@")
    _rust_toolchain_repo(name = toolchain_repo_name, pigweed_repo_name = pigweed_repo_name)
    for host in HOSTS:
        cipd_os = host["os"]
        if cipd_os == "macos":
            cipd_os = "mac"

        cipd_repository(
            name = "rust_toolchain_host_{}_{}".format(host["os"], host["cpu"]),
            build_file = "{}//pw_toolchain/rust:rust_toolchain.BUILD".format(pigweed_repo_name),
            path = "fuchsia/third_party/rust/host/{}-{}".format(cipd_os, host["cipd_arch"]),
            tag = cipd_tag,
        )

        cipd_repository(
            name = "rust_toolchain_target_{}".format(host["triple"]),
            build_file = "{}//pw_toolchain/rust:rust_stdlib.BUILD".format(pigweed_repo_name),
            path = "fuchsia/third_party/rust/target/{}".format(host["triple"]),
            tag = cipd_tag,
        )

    for target in EXTRA_TARGETS:
        cipd_repository(
            name = "rust_toolchain_target_{}".format(target["triple"]),
            build_file = "{}//pw_toolchain/rust:rust_stdlib.BUILD".format(pigweed_repo_name),
            path = "fuchsia/third_party/rust/target/{}".format(target["triple"]),
            tag = cipd_tag,
        )

# buildifier: disable=unnamed-macro
def pw_rust_register_toolchains(pigweed_repo_name = "@pigweed"):
    """Register Rust Toolchains

    Args:
      pigweed_repo_name: The name of the pigweed used to reference build files
        for the registered repositories.  Defaults to "@pigweed".

    For this registration to be valid one must
    1. Call `pw_rust_register_toolchain_and_target_repos(tag)` previously in the
       WORKSPACE file.
    2. Call `pw_rust_declare_toolchain_targets()` from
       `//pw_toolchain/rust/BUILD.bazel`.
    """

    toolchain_repo = "{}_rust_toolchain_repo".format(pigweed_repo_name)

    for channel in CHANNELS:
        for host in HOSTS:
            native.register_toolchains(
                "{}//:host_rust_toolchain_{}_{}_{}".format(toolchain_repo, host["os"], host["cpu"], channel["name"]),
                "{}//:host_rust_analyzer_toolchain_{}_{}_{}".format(toolchain_repo, host["os"], host["cpu"], channel["name"]),
            )
            for target in EXTRA_TARGETS:
                native.register_toolchains(
                    "{}//:{}_{}_rust_toolchain_{}_{}_{}".format(toolchain_repo, host["os"], host["cpu"], target["triple"], target["cpu"], channel["name"]),
                )

# buildifier: disable=unnamed-macro
def pw_rust_declare_toolchain_targets():
    """Declare rust toolchain targets"""
    for channel in CHANNELS:
        for host in HOSTS:
            _pw_rust_host_toolchain(
                name = "host_rust_toolchain_{}_{}_{}".format(host["os"], host["cpu"], channel["name"]),
                analyzer_toolchain_name = "host_rust_analyzer_toolchain_{}_{}_{}".format(host["os"], host["cpu"], channel["name"]),
                compatible_with = [
                    "@platforms//cpu:{}".format(host["cpu"]),
                    "@platforms//os:{}".format(host["os"]),
                ],
                target_settings = channel["target_settings"],
                dylib_ext = host["dylib_ext"],
                target_repo = "@rust_toolchain_target_{}".format(host["triple"]),
                toolchain_repo = "@rust_toolchain_host_{}_{}".format(host["os"], host["cpu"]),
                triple = host["triple"],
                extra_rustc_flags = channel["extra_rustc_flags"],
            )
            for target in EXTRA_TARGETS:
                _pw_rust_toolchain(
                    name = "{}_{}_rust_toolchain_{}_{}_{}".format(host["os"], host["cpu"], target["triple"], target["cpu"], channel["name"]),
                    exec_triple = host["triple"],
                    target_triple = target["triple"],
                    target_repo = "@rust_toolchain_target_{}".format(target["triple"]),
                    toolchain_repo = "@rust_toolchain_host_{}_{}".format(host["os"], host["cpu"]),
                    dylib_ext = "*.so",
                    exec_compatible_with = [
                        "@platforms//cpu:{}".format(host["cpu"]),
                        "@platforms//os:{}".format(host["os"]),
                    ],
                    target_compatible_with = [
                        "@platforms//cpu:{}".format(target["cpu"]),
                    ],
                    target_settings = channel["target_settings"],
                    extra_rustc_flags = channel["extra_rustc_flags"],
                )

def _pw_rust_toolchain(
        name,
        exec_triple,
        target_triple,
        toolchain_repo,
        target_repo,
        dylib_ext,
        exec_compatible_with,
        target_compatible_with,
        target_settings,
        extra_rustc_flags):
    rust_toolchain(
        name = "{}_rust_toolchain".format(name),
        binary_ext = "",
        clippy_driver = "{}//:bin/clippy-driver".format(toolchain_repo),
        default_edition = "2021",
        dylib_ext = dylib_ext,
        exec_compatible_with = exec_compatible_with,
        exec_triple = exec_triple,
        rust_doc = "{}//:bin/rustdoc".format(toolchain_repo),
        rust_std = "{}//:rust_std".format(target_repo),
        rustc = "{}//:bin/rustc".format(toolchain_repo),
        rustc_lib = "{}//:rustc_lib".format(toolchain_repo),
        staticlib_ext = ".a",
        stdlib_linkflags = [],
        target_compatible_with = target_compatible_with,
        target_triple = target_triple,
        extra_rustc_flags = extra_rustc_flags,
        extra_exec_rustc_flags = extra_rustc_flags,
        # TODO: https://pwbug.dev/342695883 - Works around confusing
        # target_compatible_with semantics in rust_toolchain. Figure out how to
        # do better.
        tags = ["manual"],
    )
    native.toolchain(
        name = name,
        exec_compatible_with = exec_compatible_with,
        target_compatible_with = target_compatible_with,
        target_settings = target_settings,
        toolchain = ":{}_rust_toolchain".format(name),
        toolchain_type = "@rules_rust//rust:toolchain",
    )

def _pw_rust_host_toolchain(
        name,
        analyzer_toolchain_name,
        triple,
        toolchain_repo,
        target_repo,
        dylib_ext,
        compatible_with,
        target_settings,
        extra_rustc_flags):
    _pw_rust_toolchain(
        name = name,
        exec_triple = triple,
        target_triple = triple,
        toolchain_repo = toolchain_repo,
        target_repo = target_repo,
        dylib_ext = dylib_ext,
        exec_compatible_with = compatible_with,
        target_compatible_with = compatible_with,
        target_settings = target_settings,
        extra_rustc_flags = extra_rustc_flags,
    )

    rust_analyzer_toolchain(
        name = "{}_rust_analyzer_toolchain".format(analyzer_toolchain_name),
        exec_compatible_with = compatible_with,
        proc_macro_srv = "{}//:libexec/rust-analyzer-proc-macro-srv".format(toolchain_repo),
        rustc = "{}//:bin/rustc".format(toolchain_repo),
        rustc_srcs = "{}//:rustc_srcs".format(toolchain_repo),
        target_compatible_with = compatible_with,
        visibility = ["//visibility:public"],
    )

    native.toolchain(
        name = analyzer_toolchain_name,
        exec_compatible_with = compatible_with,
        target_compatible_with = compatible_with,
        target_settings = target_settings,
        toolchain = ":{}_rust_analyzer_toolchain".format(analyzer_toolchain_name),
        toolchain_type = "@rules_rust//rust/rust_analyzer:toolchain_type",
    )
