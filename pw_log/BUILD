# Copyright 2020 The Pigweed Authors
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
PW_LOG_BACKEND = "//pw_log_basic"

pw_cc_library(
    name = "facade",
    hdrs = [
        "public/pw_log/levels.h",
        "public/pw_log/log.h",
        "public/pw_log/options.h",
        "public/pw_log/short.h",
        "public/pw_log/shorter.h",
    ],
    includes = ["public"],
    deps = [
        PW_LOG_BACKEND + ":headers",
        "//pw_preprocessor",
    ],
)

pw_cc_library(
    name = "pw_log",
    deps = [
        ":facade",
        PW_LOG_BACKEND + ":headers",
    ],
)

pw_cc_library(
    name = "backend",
    deps = [
        PW_LOG_BACKEND,
    ],
)

pw_cc_test(
    name = "test",
    srcs = [
        "basic_log_test.cc",
        "basic_log_test_plain_c.c",
    ],
    deps = [
        ":backend",
        ":facade",
        ":pw_log",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)
