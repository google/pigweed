# Copyright 2019 The Pigweed Authors
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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

load(
    "//pw_build:pigweed.bzl",
    "pw_default_copts",
    "pw_default_linkopts",
    "pw_test",
)

cc_library(
    name = "pw_string",
    srcs = ["type_to_string.cc"],
    hdrs = [
        "public/pw_string/to_string.h",
        "public/pw_string/type_to_string.h",
    ],
    copts = pw_default_copts(),
    linkopts = pw_default_linkopts(),
    deps = [
        "//pw_preprocessor",
        "//pw_span",
        "//pw_status",
    ],
)

pw_test(
    name = "type_to_string_test",
    srcs = ["type_to_string_test.cc"],
    copts = pw_default_copts(),
    linkopts = pw_default_linkopts(),
    deps = ["//pw_string"],
)

pw_test(
    name = "to_string_test",
    srcs = ["to_string_test.cc"],
    copts = pw_default_copts(),
    linkopts = pw_default_linkopts(),
    deps = ["//pw_string"],
)
