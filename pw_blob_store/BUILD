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
    name = "pw_blob_store",
    srcs = [ "blob_store.cc" ],
    hdrs = [
        "public/pw_blob_store/blob_store.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_checksum",
        "//pw_containers",
        "//pw_log",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_test(
    name = "blob_store_test",
    srcs = [
        "blob_store_test.cc",
    ],
    deps = [
        ":pw_blob_store",
        "//pw_kvs:crc16",
        "//pw_kvs:fake_flash",
        "//pw_kvs:fake_flash_test_key_value_store",
        "//pw_log",
        "//pw_random",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "blob_store_chunk_write_test",
    srcs = [
        "blob_store_chunk_write_test.cc",
    ],
    deps = [
        ":pw_blob_store",
        "//pw_kvs:crc16",
        "//pw_kvs:fake_flash",
        "//pw_kvs:fake_flash_test_key_value_store",
        "//pw_log",
        "//pw_random",
        "//pw_unit_test",
    ],
)
pw_cc_test(
    name = "blob_store_deferred_write_test",
    srcs = [
        "blob_store_deferred_write_test.cc",
    ],
    deps = [
        ":pw_blob_store",
        "//pw_kvs:crc16",
        "//pw_kvs:fake_flash",
        "//pw_kvs:fake_flash_test_key_value_store",
        "//pw_log",
        "//pw_random",
        "//pw_unit_test",
    ],
)
