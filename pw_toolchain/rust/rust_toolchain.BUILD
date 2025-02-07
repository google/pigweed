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

# This is produced by running the following command on the stderr of the
# compilation of libcore:
#     cat /path/to/actions/stderr-2 | grep "error\[E0725\]" | sed -r 's/.* `/    "/' | sed -r 's/` .*/",/' | sort
_LIBCORE_UNSTABLE_FEATURES = [
    "abi_unadjusted",
    "adt_const_params",
    "allow_internal_unsafe",
    "allow_internal_unstable",
    "arm_target_feature",
    "array_ptr_get",
    "asm_experimental_arch",
    "auto_traits",
    "avx512_target_feature",
    "bigint_helper_methods",
    "bstr",
    "bstr_internals",
    "cfg_sanitize",
    "cfg_target_has_atomic",
    "cfg_target_has_atomic_equal_alignment",
    "cfg_ub_checks",
    "const_carrying_mul_add",
    "const_eval_select",
    "const_precise_live_drops",
    "const_trait_impl",
    "core_intrinsics",
    "coverage_attribute",
    "decl_macro",
    "deprecated_suggestion",
    "doc_cfg",
    "doc_cfg_hide",
    "doc_notable_trait",
    "extern_types",
    "f128",
    "f16",
    "freeze_impls",
    "fundamental",
    "generic_arg_infer",
    "hexagon_target_feature",
    "if_let_guard",
    "internal_impls_macro",
    "intra_doc_pointers",
    "intrinsics",
    "ip",
    "is_ascii_octdigit",
    "lang_items",
    "lazy_get",
    "let_chains",
    "link_cfg",
    "link_llvm_intrinsics",
    "loongarch_target_feature",
    "macro_metavar_expr",
    "marker_trait_attr",
    "min_specialization",
    "mips_target_feature",
    "multiple_supertrait_upcastable",
    "must_not_suspend",
    "negative_impls",
    "never_type",
    "no_core",
    "no_sanitize",
    "non_null_from_ref",
    "offset_of_enum",
    "optimize_attribute",
    "panic_internals",
    "powerpc_target_feature",
    "prelude_import",
    "ptr_alignment_type",
    "ptr_metadata",
    "repr_simd",
    "riscv_target_feature",
    "rtm_target_feature",
    "rustc_allow_const_fn_unstable",
    "rustc_attrs",
    "rustdoc_internals",
    "set_ptr_value",
    "sha512_sm_x86",
    "simd_ffi",
    "slice_as_array",
    "slice_as_chunks",
    "slice_ptr_get",
    "sse4a_target_feature",
    "staged_api",
    "stmt_expr_attributes",
    "str_internals",
    "str_split_inclusive_remainder",
    "str_split_remainder",
    "strict_provenance_lints",
    "target_feature_11",
    "tbm_target_feature",
    "trait_alias",
    "transparent_unions",
    "try_blocks",
    "ub_checks",
    "unboxed_closures",
    "unchecked_neg",
    "unchecked_shifts",
    "unsized_fn_params",
    "utf16_extra",
    "variant_count",
    "wasm_target_feature",
    "with_negative_coherence",
    "x86_amx_intrinsics",
]

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
    edition = "2021",
    rustc_flags = [
        "--cap-lints=allow",
        "-Zallow-features=" + ",".join(_LIBCORE_UNSTABLE_FEATURES),
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
