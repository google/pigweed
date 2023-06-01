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
"""Utilities for declaring Rust toolchains that are compatible with @bazel_embedded"""

load("@rules_rust//rust:toolchain.bzl", "rust_toolchain")

def pw_rust_toolchain(name, exec_cpu, exec_os, target_cpu, rust_target_triple, exec_triple, target_constraints):
    proxy_toolchain = "@rust_{}_{}__{}__stable_tools".format(exec_os, exec_cpu, rust_target_triple)

    rust_toolchain(
        name = "{}_impl".format(name),
        binary_ext = "",
        dylib_ext = ".so",
        exec_triple = exec_triple,
        os = "none",
        rust_doc = "{}//:rustdoc".format(proxy_toolchain),
        rust_std = "{}//:rust_std-{}".format(proxy_toolchain, rust_target_triple),
        rustc = "{}//:rustc".format(proxy_toolchain),
        rustc_lib = "{}//:rustc_lib".format(proxy_toolchain),
        staticlib_ext = ".a",
        stdlib_linkflags = [],
        target_triple = rust_target_triple,
    )

    native.toolchain(
        name = name,
        exec_compatible_with = [
            "@platforms//cpu:{}".format(exec_cpu),
            "@platforms//os:{}".format(exec_os),
        ],
        target_compatible_with = [
            "@platforms//cpu:{}".format(target_cpu),
        ] + target_constraints,
        toolchain = ":{}_impl".format(name),
        toolchain_type = "@rules_rust//rust:toolchain",
    )
