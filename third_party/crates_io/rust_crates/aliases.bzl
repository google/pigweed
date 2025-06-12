# Copyright 2025 The Pigweed Authors
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
"""Utility to generate crate aliases"""

# THIS FILE IS AUTO-GENERATED. DO NOT EDIT MANUALLY!!!
# See //README.md for information on updating

def make_crate_aliases(name):
    """Utility to generate crate aliases

    Args:
      name: unused
    """

    native.alias(
        name = "anyhow",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:anyhow",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "askama",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:askama",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "bitfield-struct",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:bitfield-struct",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "clap",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:clap",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "cliclack",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:cliclack",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "compiler_builtins",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:compiler_builtins",
            ":std": "@crates_std//:compiler_builtins",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "cortex-m",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:cortex-m",
            ":std": "@crates_std//:cortex-m",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "cortex-m-rt",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:cortex-m-rt",
            ":std": "@crates_std//:cortex-m-rt",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "cortex-m-semihosting",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:cortex-m-semihosting",
            ":std": "@crates_std//:cortex-m-semihosting",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "embedded-io",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:embedded-io",
            ":std": "@crates_std//:embedded-io",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "hashlink",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:hashlink",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "intrusive-collections",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:intrusive-collections",
            ":std": "@crates_std//:intrusive-collections",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "libc",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:libc",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "nix",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:nix",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "nom",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:nom",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "object",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:object",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "panic-halt",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:panic-halt",
            ":std": "@crates_std//:panic-halt",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "paste",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:paste",
            ":std": "@crates_std//:paste",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "proc-macro2",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:proc-macro2",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "quote",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:quote",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "riscv",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:riscv",
            ":std": "@crates_std//:riscv",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "riscv-rt",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:riscv-rt",
            ":std": "@crates_std//:riscv-rt",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "riscv-semihosting",
        target_compatible_with = select({
            ":no_std": [],
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:riscv-semihosting",
            ":std": "@crates_std//:riscv-semihosting",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "rp235x-hal",
        target_compatible_with = select({
            ":no_std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":no_std": "@crates_no_std//:rp235x-hal",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "rustc-demangle",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:rustc-demangle",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "serde",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:serde",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "serde_json",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:serde_json",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "syn",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:syn",
        }),
        visibility = ["//visibility:public"],
    )
    native.alias(
        name = "toml",
        target_compatible_with = select({
            ":std": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        actual = select({
            ":std": "@crates_std//:toml",
        }),
        visibility = ["//visibility:public"],
    )
