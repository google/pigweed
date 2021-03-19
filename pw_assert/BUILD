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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

# TODO(pwbug/101): Need to add support for facades/backends to Bazel.
PW_ASSERT_BACKEND = "//pw_assert_basic"

pw_cc_library(
    name = "facade",
    hdrs = [
        "public/pw_assert/assert.h",
        "public/pw_assert/check.h",
        "public/pw_assert/internal/check_impl.h",
        "public/pw_assert/light.h",
        "public/pw_assert/options.h",
        "public/pw_assert/short.h",
    ],
    includes = ["public"],
    deps = [
        PW_ASSERT_BACKEND + ":headers",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "pw_assert",
    deps = [
        ":facade",
        PW_ASSERT_BACKEND + ":headers",
        PW_ASSERT_BACKEND,
    ],
)

pw_cc_library(
    name = "backend",
    deps = [
        PW_ASSERT_BACKEND,
    ],
)

pw_cc_test(
    name = "assert_facade_test",
    srcs = [
        "assert_facade_test.cc",
        "fake_backend.cc",
        "light_test.cc",
        "public/pw_assert/internal/assert_impl.h",
        "pw_assert_test/fake_backend.h",
    ],
    deps = [
        ":facade",
        "//pw_preprocessor",
        "//pw_span",
        "//pw_string",
        "//pw_unit_test",
        PW_ASSERT_BACKEND,
    ],
)

pw_cc_test(
    name = "assert_backend_compile_test",
    srcs = [
        "assert_backend_compile_test.cc",
        "assert_backend_compile_test_c.c",
    ],
    deps = [
        ":backend",
        ":facade",
        ":pw_assert",
        "//pw_unit_test",
    ],
)
