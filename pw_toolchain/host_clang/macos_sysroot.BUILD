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

load("@bazel_skylib//rules/directory:directory.bzl", "directory")
load("@bazel_skylib//rules/directory:subdirectory.bzl", "subdirectory")
load("@rules_cc//cc/toolchains/args:sysroot.bzl", "cc_sysroot")

package(default_visibility = ["//visibility:public"])

cc_sysroot(
    name = "sysroot",
    sysroot = ":root",
    data = glob([
        "usr/lib/*.tbd",
        "System/Library/Frameworks/*/*.tbd",
    ]),
    allowlist_include_directories = [
        ":usr-include",
        ":CoreFoundation.framework-Headers",
        ":IOKit.framework-Headers",
        ":Security.framework-Headers",
    ],
)

directory(
    name = "root",
    srcs = glob([
        "usr/include/**/*",
        "usr/lib/**/*",
        "System/Library/Frameworks/CoreFoundation.framework/**/*",
        "System/Library/Frameworks/IOKit.framework/**/*",
        "System/Library/Frameworks/Security.framework/**/*",
    ]),
)

subdirectory(
    name = "usr-include",
    parent = ":root",
    path = "usr/include",
)

# Eventually, it's probably better to expose frameworks as cc_args with a
# `-framework CoreFoundation` flag and a single subdirectory in
# `allowlist_include_directories`. This better isolates usages and ensures
# things are properly linked.
subdirectory(
    name = "CoreFoundation.framework-Headers",
    parent = ":root",
    path = "System/Library/Frameworks/CoreFoundation.framework/Headers",
)

subdirectory(
    name = "IOKit.framework-Headers",
    parent = ":root",
    path = "System/Library/Frameworks/IOKit.framework/Headers",
)

subdirectory(
    name = "Security.framework-Headers",
    parent = ":root",
    path = "System/Library/Frameworks/Security.framework/Headers",
)
