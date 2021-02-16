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

load(
    "//pw_build:pigweed.bzl",
    "pw_cc_library",
    "pw_cc_test",
)
load(
    "@bazel_embedded//toolchains/tools/include_tools:defs.bzl",
    "cc_injected_toolchain_header_library",
    "cc_polyfill_toolchain_library",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

cc_injected_toolchain_header_library(
    name = "toolchain_injected_headers",
    hdrs = ["language_features.h"],
)

cc_polyfill_toolchain_library(
    name = "toolchain_polyfill_overrides",
    hdrs = [
        "language_features.h",
        "public_overrides/array",
        "public_overrides/assert.h",
        "public_overrides/bit",
        "public_overrides/cstddef",
        "public_overrides/iterator",
        "public_overrides/type_traits",
        "public_overrides/utility",
        "standard_library_public/pw_polyfill/standard_library/array.h",
        "standard_library_public/pw_polyfill/standard_library/assert.h",
        "standard_library_public/pw_polyfill/standard_library/bit.h",
        "standard_library_public/pw_polyfill/standard_library/cstddef.h",
        "standard_library_public/pw_polyfill/standard_library/iterator.h",
        "standard_library_public/pw_polyfill/standard_library/namespace.h",
        "standard_library_public/pw_polyfill/standard_library/type_traits.h",
        "standard_library_public/pw_polyfill/standard_library/utility.h",
    ],
    system_includes = [
        "public_overrides",
        "public",
        "standard_library_public",
    ],
)

pw_cc_library(
    name = "pw_polyfill",
    hdrs = [
        "public/pw_polyfill/language_feature_macros.h",
        "public/pw_polyfill/standard.h",
    ],
    includes = ["public"],
    deps = [":standard_library"],
)

pw_cc_library(
    name = "overrides",
    srcs = ["language_features.h"],
    hdrs = [
        "public_overrides/array",
        "public_overrides/assert.h",
        "public_overrides/bit",
        "public_overrides/cstddef",
        "public_overrides/iterator",
        "public_overrides/type_traits",
        "public_overrides/utility",
    ],
    copts = [
        "-include",
        "language_features.h",
    ],
    includes = ["public_overrides"],
    deps = [":standard_library"],
)

pw_cc_library(
    name = "standard_library",
    hdrs = [
        "standard_library_public/pw_polyfill/standard_library/array.h",
        "standard_library_public/pw_polyfill/standard_library/assert.h",
        "standard_library_public/pw_polyfill/standard_library/bit.h",
        "standard_library_public/pw_polyfill/standard_library/cstddef.h",
        "standard_library_public/pw_polyfill/standard_library/iterator.h",
        "standard_library_public/pw_polyfill/standard_library/namespace.h",
        "standard_library_public/pw_polyfill/standard_library/type_traits.h",
        "standard_library_public/pw_polyfill/standard_library/utility.h",
    ],
    includes = ["standard_library_public"],
    visibility = ["//visibility:private"],
)

pw_cc_test(
    name = "default_cpp_test",
    srcs = [
        "test.cc",
    ],
    deps = [
        ":pw_polyfill",
        ":standard_library",
        "//pw_unit_test",
    ],
)
