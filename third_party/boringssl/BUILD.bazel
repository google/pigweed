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
#

load(
    "//pw_build:pigweed.bzl",
    "pw_cc_library",
    "pw_cc_test",
)

pw_cc_library(
    name = "sysdeps",
    srcs = ["crypto_sysrand.cc"],
    hdrs = ["sysdeps/sys/socket.h"],
    includes = ["sysdeps"],
)

# TODO(zyecheng): Add build recipes for BoringSSL

# TODO(zyecheng): Add boringssl library dependency once its build recipe is ready.
pw_cc_library(
    name = "test_server",
    srcs = ["test_server.cc"],
    hdrs = ["public/boringssl/test/test_server.h"],
    includes = ["public"],
)

pw_cc_test(
    name = "test_server_test",
    srcs = ["test_server_test.cc"],
    deps = [":test_server"],
)
