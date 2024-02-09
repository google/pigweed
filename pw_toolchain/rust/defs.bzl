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

HOSTS = [
    {
        "cpu": "aarch64",
        "cipd_arch": "arm64",
        "os": "linux",
        "triple": "aarch64-unknown-linux-gnu",
        "dylib_ext": ".so",
    },
    {
        "cpu": "x86_64",
        "cipd_arch": "amd64",
        "os": "linux",
        "triple": "x86_64-unknown-linux-gnu",
        "dylib_ext": ".so",
    },
    {
        "cpu": "aarch64",
        "cipd_arch": "arm64",
        "os": "macos",
        "triple": "aarch64-apple-darwin",
        "dylib_ext": ".dylib",
    },
    {
        "cpu": "x86_64",
        "cipd_arch": "amd64",
        "os": "macos",
        "triple": "x86_64-apple-darwin",
        "dylib_ext": ".dylib",
    },
]

EXTRA_TARGETS = [
    {
        "cpu": "armv6-m",
        "triple": "thumbv6m-none-eabi",
    },
    {
        "cpu": "armv7-m",
        "triple": "thumbv7m-none-eabi",
    },
    {
        "cpu": "armv7e-m",
        "triple": "thumbv7m-none-eabi",
    },
]

CHANNELS = [
    {
        "name": "nightly",
        "extra_rustc_flags": [],
        "target_settings": ["@rules_rust//rust/toolchain/channel:nightly"],
    },
    {
        "name": "stable",
        # In order to approximate a stable toolchain with our nightly one, we
        # disable experimental features with the exception of `proc_macro_span`
        # because the `proc-marcro2` automatically detects the toolchain
        # as nightly and dynamically uses this feature.
        "extra_rustc_flags": ["-Zallow-features=proc_macro_span"],
        "target_settings": ["@rules_rust//rust/toolchain/channel:stable"],
    },
]

# buildifier: disable=unnamed-macro
def pw_rust_register_toolchain_and_target_repos(cipd_tag):
    """Declare and register CIPD repos for Rust toolchain and target rupport.

    Args:
      cipd_tag: Tag with which to select specific package versions.
    """
    for host in HOSTS:
        cipd_os = host["os"]
        if cipd_os == "macos":
            cipd_os = "mac"

        cipd_repository(
            name = "rust_toolchain_host_{}_{}".format(host["os"], host["cpu"]),
            build_file = "//pw_toolchain/rust:rust_toolchain.BUILD",
            path = "fuchsia/third_party/rust/host/{}-{}".format(cipd_os, host["cipd_arch"]),
            tag = cipd_tag,
        )

        cipd_repository(
            name = "rust_toolchain_target_{}".format(host["triple"]),
            build_file = "//pw_toolchain/rust:rust_stdlib.BUILD",
            path = "fuchsia/third_party/rust/target/{}".format(host["triple"]),
            tag = cipd_tag,
        )

    for target in EXTRA_TARGETS:
        cipd_repository(
            name = "rust_toolchain_target_{}".format(target["triple"]),
            build_file = "//pw_toolchain/rust:rust_stdlib.BUILD",
            path = "fuchsia/third_party/rust/target/{}".format(target["triple"]),
            tag = cipd_tag,
        )

# buildifier: disable=unnamed-macro
def pw_rust_register_toolchains():
    """Register Rust Toolchains

    For this registration to be valid one must
    1. Call `pw_rust_register_toolchain_and_target_repos(tag)` pervisouly in the
       WORKSPACE file.
    2. Call `pw_rust_declare_toolchain_targets()` from
       `//pw_toolchain/rust/BUILD.bazel`.
    """
    for channel in CHANNELS:
        for host in HOSTS:
            native.register_toolchains(
                "//pw_toolchain/rust:host_rust_toolchain_{}_{}_{}".format(host["os"], host["cpu"], channel["name"]),
                "//pw_toolchain/rust:host_rust_analyzer_toolchain_{}_{}_{}".format(host["os"], host["cpu"], channel["name"]),
            )
            for target in EXTRA_TARGETS:
                native.register_toolchains(
                    "//pw_toolchain/rust:{}_{}_rust_toolchain_{}_{}_{}".format(host["os"], host["cpu"], target["triple"], target["cpu"], channel["name"]),
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
