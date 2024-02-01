# Copyright 2024 The Pigweed Authors
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
"""A list of features that should be built into every toolchain."""

visibility(["//cc_toolchain/private", "//cc_toolchain/tests/..."])

BUILTIN_FEATURES = [
    "//features/well_known:opt",
    "//features/well_known:dbg",
    "//features/well_known:fastbuild",
    "//features/well_known:static_linking_mode",
    "//features/well_known:dynamic_linking_mode",
    "//features/well_known:per_object_debug_info",
    "//features/well_known:supports_start_end_lib",
    "//features/well_known:supports_interface_shared_libraries",
    "//features/well_known:supports_dynamic_linker",
    "//features/well_known:static_link_cpp_runtimes",
    "//features/well_known:supports_pic",
    "//features/legacy:legacy_compile_flags",
    "//features/legacy:default_compile_flags",
    "//features/legacy:dependency_file",
    "//features/legacy:pic",
    "//features/legacy:preprocessor_defines",
    "//features/legacy:includes",
    "//features/legacy:include_paths",
    "//features/legacy:fdo_instrument",
    "//features/legacy:fdo_optimize",
    "//features/legacy:cs_fdo_instrument",
    "//features/legacy:cs_fdo_optimize",
    "//features/legacy:fdo_prefetch_hints",
    "//features/legacy:autofdo",
    "//features/legacy:build_interface_libraries",
    "//features/legacy:dynamic_library_linker_tool",
    "//features/legacy:shared_flag",
    "//features/legacy:linkstamps",
    "//features/legacy:output_execpath_flags",
    "//features/legacy:runtime_library_search_directories",
    "//features/legacy:library_search_directories",
    "//features/legacy:archiver_flags",
    "//features/legacy:libraries_to_link",
    "//features/legacy:force_pic_flags",
    "//features/legacy:user_link_flags",
    "//features/legacy:legacy_link_flags",
    "//features/legacy:static_libgcc",
    "//features/legacy:fission_support",
    "//features/legacy:strip_debug_symbols",
    "//features/legacy:coverage",
    "//features/legacy:llvm_coverage_map_format",
    "//features/legacy:gcc_coverage_map_format",
    "//features/legacy:fully_static_link",
    "//features/legacy:user_compile_flags",
    "//features/legacy:sysroot",
    "//features/legacy:unfiltered_compile_flags",
    "//features/legacy:linker_param_file",
    "//features/legacy:compiler_input_flags",
    "//features/legacy:compiler_output_flags",
]
