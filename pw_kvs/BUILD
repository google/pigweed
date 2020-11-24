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
    name = "pw_kvs",
    srcs = [
        "alignment.cc",
        "checksum.cc",
        "entry.cc",
        "entry_cache.cc",
        "flash_memory.cc",
        "format.cc",
        "key_value_store.cc",
        "public/pw_kvs/internal/entry.h",
        "public/pw_kvs/internal/entry_cache.h",
        "public/pw_kvs/internal/hash.h",
        "public/pw_kvs/internal/key_descriptor.h",
        "public/pw_kvs/internal/sectors.h",
        "public/pw_kvs/internal/span_traits.h",
        "pw_kvs_private/config.h",
        "sectors.cc",
    ],
    hdrs = [
        "public/pw_kvs/alignment.h",
        "public/pw_kvs/checksum.h",
        "public/pw_kvs/crc16_checksum.h",
        "public/pw_kvs/flash_memory.h",
        "public/pw_kvs/format.h",
        "public/pw_kvs/io.h",
        "public/pw_kvs/key.h",
        "public/pw_kvs/key_value_store.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_assert",
        "//pw_checksum",
        "//pw_containers",
        "//pw_log",
        "//pw_log:facade",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "crc16",
    hdrs = [
        "public/pw_kvs/crc16_checksum.h",
    ],
    deps = [
        ":pw_kvs",
        "//pw_checksum",
        "//pw_span",
    ],
)

pw_cc_library(
    name = "fake_flash",
    srcs = [
        "fake_flash_memory.cc",
    ],
    hdrs = [
        "public/pw_kvs/fake_flash_memory.h",
    ],
    deps = [
        ":pw_kvs",
        "//pw_containers",
        "//pw_log",
        "//pw_log:facade",
        "//pw_span",
        "//pw_status",
    ],
)

pw_cc_library(
    name = "fake_flash_small_partition",
    srcs = [
        "fake_flash_test_partition.cc",
    ],
    hdrs = [
        "public/pw_kvs/flash_test_partition.h",
    ],
    deps = [
        ":fake_flash",
        ":pw_kvs",
    ],
)

pw_cc_library(
    name = "fake_flash_64_aligned_partition",
    srcs = [
        "fake_flash_test_partition.cc",
    ],
    hdrs = [
        "public/pw_kvs/flash_test_partition.h",
    ],
    defines = ["PW_FLASH_TEST_ALIGNMENT=64"],
    deps = [
        ":fake_flash",
        ":pw_kvs",
    ],
)

pw_cc_library(
    name = "fake_flash_256_aligned_partition",
    srcs = [
        "fake_flash_test_partition.cc",
    ],
    hdrs = [
        "public/pw_kvs/flash_test_partition.h",
    ],
    defines = ["PW_FLASH_TEST_ALIGNMENT=256"],
    deps = [
        ":fake_flash",
        ":pw_kvs",
    ],
)

pw_cc_library(
    name = "fake_flash_test_key_value_store",
    srcs = [
        "fake_flash_test_key_value_store.cc",
    ],
    hdrs = [
        "public/pw_kvs/test_key_value_store.h",
    ],
    deps = [
        ":crc16",
        ":fake_flash",
        ":pw_kvs",
    ],
)

pw_cc_library(
    name = "test_utils",
    hdrs = [
        "pw_kvs_private/byte_utils.h",
    ],
    includes = ["public"],
    visibility = ["//visibility:private"],
)

pw_cc_library(
    name = "test_partition",
    srcs = [
        "flash_partition_with_stats.cc",
    ],
    hdrs = [
        "public/pw_kvs/flash_partition_with_stats.h",
    ],
    includes = ["public"],
    visibility = ["//visibility:private"],
    deps = [
        "//pw_containers",
        "//pw_kvs",
        "//pw_log",
        "//pw_log:facade",
        "//pw_status",
    ],
)

pw_cc_test(
    name = "alignment_test",
    srcs = [
        "alignment_test.cc",
    ],
    deps = [
        ":pw_kvs",
        "//pw_status",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "checksum_test",
    srcs = ["checksum_test.cc"],
    deps = [
        ":crc16",
        ":pw_kvs",
        "//pw_checksum",
        "//pw_log",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "converts_to_span_test",
    srcs = ["converts_to_span_test.cc"],
    deps = [":pw_kvs"],
)

pw_cc_test(
    name = "entry_test",
    srcs = [
        "entry_test.cc",
    ],
    deps = [
        ":pw_kvs",
        ":test_utils",
        "//pw_log:backend",
        "//pw_span",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "entry_cache_test",
    srcs = ["entry_cache_test.cc"],
    deps = [
        ":pw_kvs",
        ":test_utils",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "flash_partition_small_test",
    srcs = ["flash_partition_test.cc"],
    defines = ["PW_FLASH_TEST_ITERATIONS=100"],
    deps = [
        ":fake_flash_small_partition",
        ":pw_kvs",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "flash_partition_64_alignment_test",
    srcs = ["flash_partition_test.cc"],
    defines = ["PW_FLASH_TEST_ITERATIONS=100"],
    deps = [
        ":fake_flash_64_aligned_partition",
        ":pw_kvs",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "flash_partition_256_alignment_test",
    srcs = ["flash_partition_test.cc"],
    defines = ["PW_FLASH_TEST_ITERATIONS=100"],
    deps = [
        ":fake_flash_256_aligned_partition",
        ":pw_kvs",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_test",
    srcs = [
        "key_test.cc",
    ],
    deps = [
        ":pw_kvs",
        "//pw_status",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_test",
    srcs = ["key_value_store_test.cc"],
    deps = [
        ":crc16",
        ":pw_kvs",
        ":test_utils",
        "//pw_checksum",
        "//pw_log:backend",
        "//pw_log:facade",
        "//pw_span",
        "//pw_status",
        "//pw_string",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_small_flash_test",
    srcs = ["key_value_store_initialized_test.cc"],
    deps = [
        ":crc16",
        ":fake_flash_small_partition",
        ":pw_kvs",
        ":test_utils",
        "//pw_checksum",
        "//pw_log:backend",
        "//pw_log:facade",
        "//pw_span",
        "//pw_status",
        "//pw_string",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_64_alignment_flash_test",
    srcs = ["key_value_store_initialized_test.cc"],
    deps = [
        ":crc16",
        ":fake_flash_64_aligned_partition",
        ":pw_kvs",
        ":test_utils",
        "//pw_checksum",
        "//pw_log:backend",
        "//pw_log:facade",
        "//pw_span",
        "//pw_status",
        "//pw_string",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_256_alignment_flash_test",
    srcs = ["key_value_store_initialized_test.cc"],
    deps = [
        ":crc16",
        ":fake_flash_256_aligned_partition",
        ":pw_kvs",
        ":test_utils",
        "//pw_checksum",
        "//pw_log:backend",
        "//pw_log:facade",
        "//pw_span",
        "//pw_status",
        "//pw_string",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "fake_flash_test_key_value_store_test",
    srcs = ["test_key_value_store_test.cc"],
    deps = [
        ":crc16",
        ":fake_flash_test_key_value_store",
        ":pw_kvs",
        "//pw_log:backend",
        "//pw_status",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_binary_format_test",
    srcs = [
        "key_value_store_binary_format_test.cc",
    ],
    deps = [
        ":crc16",
        ":pw_kvs",
        ":test_utils",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_fuzz_test",
    srcs = ["key_value_store_fuzz_test.cc"],
    deps = [
        ":crc16",
        ":pw_kvs",
        ":test_partition",
        ":test_utils",
        "//pw_checksum",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_map_test",
    srcs = ["key_value_store_map_test.cc"],
    deps = [
        ":crc16",
        ":pw_kvs",
        ":test_partition",
        ":test_utils",
        "//pw_checksum",
        "//pw_log:backend",
        "//pw_log:facade",
        "//pw_span",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "sectors_test",
    srcs = ["sectors_test.cc"],
    deps = [
        ":pw_kvs",
        ":test_utils",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "key_value_store_wear_test",
    srcs = [
        "key_value_store_wear_test.cc",
    ],
    deps = [
        ":pw_kvs",
        ":test_partition",
        ":test_utils",
        "//pw_log:backend",
        "//pw_unit_test",
    ],
)
