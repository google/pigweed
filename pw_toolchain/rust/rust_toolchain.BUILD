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

load("@pigweed//pw_toolchain/rust:no_stdlibs.bzl", "build_with_core_only", "build_with_no_stdlibs")
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_stdlib_filegroup")

exports_files(glob(["**"]))

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "rustc_lib",
    srcs = glob(
        ["lib/*.so"],
        # For non-linux operating systems, this comes up empty.
        allow_empty = True,
    ),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "rustc_srcs",
    srcs = glob(["lib/rustlib/src/rust/**/*"]),
    visibility = ["//visibility:public"],
)

rust_library(
    name = "libcore",
    srcs = glob([
        "lib/rustlib/src/rust/library/core/src/**/*.rs",
        "lib/rustlib/src/rust/library/stdarch/crates/core_arch/src/**/*.rs",
        "lib/rustlib/src/rust/library/portable-simd/crates/core_simd/src/**/*.rs",
    ]),
    compile_data = glob([
        "lib/rustlib/src/rust/library/core/src/**/*.md",
        "lib/rustlib/src/rust/library/stdarch/crates/core_arch/src/**/*.md",
        "lib/rustlib/src/rust/library/portable-simd/crates/core_simd/src/**/*.md",
    ]),
    crate_features = ["stdsimd"],
    crate_name = "core",
    edition = "2024",
    rustc_flags = [
        "--cap-lints=allow",
    ],
)

rust_stdlib_filegroup(
    name = "rust_libs_none",
    srcs = [],
    visibility = ["//visibility:public"],
)

build_with_no_stdlibs(
    name = "rust_libs_core_files",
    visibility = ["//visibility:public"],
    deps = [
        ":libcore",
    ],
)

build_with_core_only(
    name = "rust_libs_compiler_builtin_files",
    visibility = ["//visibility:public"],
    deps = [
        "@rust_crates//:compiler_builtins",
    ],
)

rust_stdlib_filegroup(
    name = "rust_libs_core_only",
    srcs = [
        ":rust_libs_core_files",
    ],
    visibility = ["//visibility:public"],
)

rust_stdlib_filegroup(
    name = "rust_libs_core",
    srcs = [
        ":rust_libs_compiler_builtin_files",
        ":rust_libs_core_files",
    ],
    visibility = ["//visibility:public"],
)
