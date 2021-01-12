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

pw_cc_library(
    name = "pw_containers",
    deps = [
        ":flat_map",
        ":vector",
        ":intrusive_list",
    ],
)

pw_cc_library(
    name = "intrusive_list",
    deps = [ "//pw_assert" ],
    srcs = [
        "intrusive_list.cc",
        "public/pw_containers/internal/intrusive_list_impl.h",
    ],
    hdrs = [
        "public/pw_containers/intrusive_list.h",
    ],
    includes = ["public"],
)

pw_cc_library(
    name = "vector",
    hdrs = [
        "public/pw_containers/vector.h",
    ],
    includes = ["public"],
)

pw_cc_library(
    name = "flat_map",
    hdrs = [
        "public/pw_containers/flat_map.h",
    ],
    includes = ["public"],
)

pw_cc_test(
    name = "flat_map_test",
    srcs = [
        "flat_map_test.cc",
    ],
    deps = [
        ":pw_containers",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "vector_test",
    srcs = [
        "vector_test.cc",
    ],
    deps = [
        ":pw_containers",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "intrusive_list_test",
    srcs = [
        "intrusive_list_test.cc",
    ],
    deps = [
        ":intrusive_list",
        "//pw_unit_test",
    ],
)
