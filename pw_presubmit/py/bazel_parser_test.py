#!/usr/bin/env python3
# Copyright 2022 The Pigweed Authors
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
"""Tests for bazel_parser."""

from pathlib import Path
import tempfile
import unittest

from pw_presubmit import bazel_parser

# This is a real Bazel failure, trimmed slightly.
_REAL_TEST_INPUT = """
Starting local Bazel server and connecting to it...
WARNING: --verbose_explanations has no effect when --explain=<file> is not enabled
Loading:
Loading: 0 packages loaded
Analyzing: 1362 targets (197 packages loaded)
Analyzing: 1362 targets (197 packages loaded, 0 targets configured)
INFO: Analyzed 1362 targets (304 packages loaded, 15546 targets configured).

INFO: Found 1362 targets...
[6 / 124] [Prepa] BazelWorkspaceStatusAction stable-status.txt
[747 / 1,548] Compiling pw_kvs/entry.cc; 0s linux-sandbox ... (3 actions, ...
ERROR: /usr/local/google/home/mohrr/pigweed/pigweed/pw_kvs/BUILD.bazel:25:14: Compiling pw_kvs/entry.cc failed: (Exit 1): gcc failed: error executing command
  (cd /usr/local/google/home/mohrr/.cache/bazel/_bazel_mohrr/7e133e1f95b61... \
  exec env - \
    CPP_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    GCC_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    LD_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    NM_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    OBJDUMP_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_... \
    PATH=/usr/local/google/home/mohrr/pigweed/pigweed/out/host/host_tools:... \
    PWD=/proc/self/cwd \
  external/rules_cc_toolchain/cc_toolchain/wrappers/posix/gcc -MD -MF bazel-out/k8-fastbuild/bin/pw_kvs/_objs/pw_kvs/entry.pic.d '-frandom-seed=bazel-out/k8-fastbuild/bin/pw_kvs/_objs/pw_kvs/entry.pic.o' -fPIC -iquote . -iquote bazel-out/k8-fastbuild/bin -isystem pw_kvs/public -isystem bazel-out/k8-fastbuild/bin/pw_kvs/public -isystem pw_assert/assert_compatibility_public_overrides -isystem bazel-out/k8-fastbuild/bin/pw_assert/assert_compatibility_public_overrides -isystem pw_assert/public -isystem bazel-out/k8-fastbuild/bin/pw_assert/public -isystem pw_preprocessor/public -isystem bazel-out/k8-fastbuild/bin/pw_preprocessor/public -isystem pw_assert_basic/public -isystem bazel-out/k8-fastbuild/bin/pw_assert_basic/public -isystem pw_assert_basic/public_overrides -isystem bazel-out/k8-fastbuild/bin/pw_assert_basic/public_overrides -isystem pw_string/public -isystem bazel-out/k8-fastbuild/bin/pw_string/public -isystem pw_span/public -isystem bazel-out/k8-fastbuild/bin/pw_span/public -isystem pw_polyfill/public -isystem bazel-out/k8-fastbuild/bin/pw_polyfill/public -isystem pw_status/public -isystem bazel-out/k8-fastbuild/bin/pw_status/public -isystem pw_result/public -isystem bazel-out/k8-fastbuild/bin/pw_result/public -isystem pw_sys_io/public -isystem bazel-out/k8-fastbuild/bin/pw_sys_io/public -isystem pw_bytes/public -isystem bazel-out/k8-fastbuild/bin/pw_bytes/public -isystem pw_containers/public -isystem bazel-out/k8-fastbuild/bin/pw_containers/public -isystem pw_checksum/public -isystem bazel-out/k8-fastbuild/bin/pw_checksum/public -isystem pw_compilation_testing/public -isystem bazel-out/k8-fastbuild/bin/pw_compilation_testing/public -isystem pw_log/public -isystem bazel-out/k8-fastbuild/bin/pw_log/public -isystem pw_log_basic/public -isystem bazel-out/k8-fastbuild/bin/pw_log_basic/public -isystem pw_log_basic/public_overrides -isystem bazel-out/k8-fastbuild/bin/pw_log_basic/public_overrides -isystem pw_stream/public -isystem bazel-out/k8-fastbuild/bin/pw_stream/public -nostdinc++ -nostdinc -isystemexternal/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/include/c++/v1 -isystemexternal/debian_stretch_amd64_sysroot/usr/local/include -isystemexternal/debian_stretch_amd64_sysroot/usr/include/x86_64-linux-gnu -isystemexternal/debian_stretch_amd64_sysroot/usr/include -isystemexternal/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/lib/clang/12.0.0 -isystemexternal/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/lib/clang/12.0.0/include -fdata-sections -ffunction-sections -no-canonical-prefixes -Wno-builtin-macro-redefined '-D__DATE__="redacted"' '-D__TIMESTAMP__="redacted"' '-D__TIME__="redacted"' -xc++ --sysroot external/debian_stretch_amd64_sysroot -O0 -fPIC -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Werror '-Wno-error=cpp' '-Wno-error=deprecated-declarations' -Wno-private-header '-std=c++17' -fno-rtti -Wnon-virtual-dtor -Wno-register -c pw_kvs/entry.cc -o bazel-out/k8-fastbuild/bin/pw_kvs/_objs/pw_kvs/entry.pic.o)
# Configuration: 752863e407a197a5b9da05cfc572e7013efd6958e856cee61d2fa474ed...
# Execution platform: @local_config_platform//:host

Use --sandbox_debug to see verbose messages from the sandbox and retain the sandbox build root for debugging
pw_kvs/entry.cc:49:20: error: no member named 'Dat' in 'pw::Status'
    return Status::Dat aLoss();
           ~~~~~~~~^
pw_kvs/entry.cc:49:23: error: expected ';' after return statement
    return Status::Dat aLoss();
                      ^
                      ;
2 errors generated.
INFO: Elapsed time: 5.662s, Critical Path: 1.01s
INFO: 12 processes: 12 internal.
FAILED: Build did NOT complete successfully
FAILED: Build did NOT complete successfully
"""

_REAL_TEST_SUMMARY = """
ERROR: /usr/local/google/home/mohrr/pigweed/pigweed/pw_kvs/BUILD.bazel:25:14: Compiling pw_kvs/entry.cc failed: (Exit 1): gcc failed: error executing command
# Execution platform: @local_config_platform//:host
pw_kvs/entry.cc:49:20: error: no member named 'Dat' in 'pw::Status'
    return Status::Dat aLoss();
           ~~~~~~~~^
pw_kvs/entry.cc:49:23: error: expected ';' after return statement
    return Status::Dat aLoss();
                      ^
                      ;
2 errors generated.
INFO: Elapsed time: 5.662s, Critical Path: 1.01s
INFO: 12 processes: 12 internal.
FAILED: Build did NOT complete successfully
FAILED: Build did NOT complete successfully
"""

_REAL_TEST_INPUT_2 = """
[4,421 / 4,540] 302 / 343 tests; Compiling pw_transfer/client_test.cc; 7s linux-sandbox ... (15 actions running)
[4,425 / 4,540] 302 / 343 tests; Compiling pw_transfer/client_test.cc; 9s linux-sandbox ... (16 actions, 15 running)
[4,426 / 4,540] 302 / 343 tests; Compiling pw_transfer/client_test.cc; 10s linux-sandbox ... (16 actions running)
[4,430 / 4,540] 302 / 343 tests; Compiling pw_rpc/raw/service_nc_test.cc; 7s linux-sandbox ... (14 actions, 13 running)
[4,439 / 4,542] 304 / 343 tests; Compiling pw_rpc/nanopb/client_server_context_test.cc; 7s linux-sandbox ... (15 actions running)
[4,443 / 4,542] 304 / 343 tests; Compiling pw_rpc/pwpb/synchronous_call_test.cc; 7s linux-sandbox ... (15 actions running)
[4,449 / 4,544] 306 / 343 tests; Compiling pw_transfer/transfer.cc; 6s linux-sandbox ... (16 actions, 15 running)
[4,458 / 4,546] 308 / 343 tests; Compiling pw_rpc/test_helpers_test.cc; 4s linux-sandbox ... (16 actions running)
[4,460 / 4,547] 309 / 343 tests; Compiling pw_rpc/test_helpers_test.cc; 5s linux-sandbox ... (16 actions running)
[4,466 / 4,548] 310 / 343 tests; Compiling pw_rpc/test_helpers_test.cc; 6s linux-sandbox ... (16 actions, 15 running)
[4,469 / 4,549] 311 / 343 tests; Compiling pw_rpc/test_helpers_test.cc; 7s linux-sandbox ... (16 actions running)
[4,478 / 4,550] 312 / 343 tests; Compiling pw_transfer/client_test.cc; 8s linux-sandbox ... (14 actions, 13 running)
[4,481 / 4,550] 312 / 343 tests; Compiling pw_transfer/client_test.cc; 9s linux-sandbox ... (16 actions, 15 running)
[4,486 / 4,551] 313 / 343 tests; Compiling pw_transfer/client_test.cc; 10s linux-sandbox ... (16 actions, 15 running)
[4,492 / 4,552] 314 / 343 tests; Compiling pw_transfer/integration_test/server.cc; 7s linux-sandbox ... (16 actions, 15 running)
[4,501 / 4,556] 318 / 343 tests; Compiling pw_transfer/atomic_file_transfer_handler_test.cc; 5s linux-sandbox ... (15 actions running)
[4,506 / 4,556] 318 / 343 tests; Compiling pw_rpc/pwpb/client_server_context_threaded_test.cc; 5s linux-sandbox ... (16 actions, 15 running)
[4,513 / 4,557] 319 / 343 tests; Compiling pw_transfer/client.cc; 6s linux-sandbox ... (15 actions, 14 running)
[4,519 / 4,557] 319 / 343 tests; Compiling pw_rpc/pwpb/client_server_context_threaded_test.cc; 5s linux-sandbox ... (16 actions, 15 running)
[4,524 / 4,558] 320 / 343 tests; Compiling pw_transfer/integration_test/client.cc; 6s linux-sandbox ... (16 actions running)
[4,533 / 4,560] 322 / 343 tests; Compiling pw_rpc/pwpb/fake_channel_output_test.cc; 4s linux-sandbox ... (16 actions running)
[4,537 / 4,560] 322 / 343 tests; Compiling pw_rpc/pwpb/fake_channel_output_test.cc; 5s linux-sandbox ... (14 actions running)
[4,544 / 4,569] 325 / 343 tests; Compiling pw_rpc/pwpb/fake_channel_output_test.cc; 6s linux-sandbox ... (15 actions running)
[4,550 / 4,569] 325 / 343 tests; Compiling pw_rpc/pwpb/method_lookup_test.cc; 5s linux-sandbox ... (14 actions, 13 running)
[4,560 / 4,571] 327 / 343 tests; Compiling pw_rpc/pwpb/server_reader_writer_test.cc; 4s linux-sandbox ... (9 actions, 8 running)
[4,566 / 4,573] 329 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 4s linux-sandbox ... (7 actions, 6 running)
[4,567 / 4,573] 329 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 5s linux-sandbox ... (6 actions running)
[4,567 / 4,573] 329 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 14s linux-sandbox ... (6 actions running)
[4,567 / 4,573] 330 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 15s linux-sandbox ... (6 actions running)
[4,568 / 4,573] 330 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 16s linux-sandbox ... (5 actions running)
[4,568 / 4,573] 330 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 26s linux-sandbox ... (5 actions running)
[4,568 / 4,573] 330 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 52s linux-sandbox ... (5 actions running)
[4,569 / 4,573] 331 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 53s linux-sandbox ... (4 actions running)
[4,570 / 4,573] 332 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 54s linux-sandbox ... (3 actions running)
[4,570 / 4,573] 332 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 65s linux-sandbox ... (3 actions running)
[4,570 / 4,573] 332 / 343 tests; Testing //pw_transfer/integration_test:cross_language_medium_read_test; 83s linux-sandbox ... (3 actions running)
FAIL: //pw_transfer/integration_test:cross_language_medium_read_test (see /b/s/w/ir/cache/bazel/_bazel_swarming/823a25200ba977576a072121498f22ec/execroot/pigweed/bazel-out/k8-fastbuild/testlogs/pw_transfer/integration_test/cross_language_medium_read_test/test.log)
INFO: From Testing //pw_transfer/integration_test:cross_language_medium_read_test:
stdout (/b/s/w/ir/cache/bazel/_bazel_swarming/823a25200ba977576a072121498f22ec/execroot/pigweed/bazel-out/_tmp/actions/stdout-2115) 1131999 exceeds maximum size of --experimental_ui_max_stdouterr_bytes=1048576 bytes; skipping
[4,572 / 4,573] 334 / 343 tests, 1 failed; Testing //pw_transfer/integration_test:expected_errors_test; 84s linux-sandbox
[4,572 / 4,573] 334 / 343 tests, 1 failed; Testing //pw_transfer/integration_test:expected_errors_test; 94s linux-sandbox
[4,572 / 4,573] 334 / 343 tests, 1 failed; Testing //pw_transfer/integration_test:expected_errors_test; 124s linux-sandbox
[4,572 / 4,573] 334 / 343 tests, 1 failed; Testing //pw_transfer/integration_test:expected_errors_test; 168s linux-sandbox
INFO: Elapsed time: 507.459s, Critical Path: 181.66s
INFO: 1206 processes: 1235 linux-sandbox, 4 worker.
INFO: Build completed, 1 test FAILED, 1206 total actions
//pw_allocator:block_test                                       (cached) PASSED in 0.1s
//pw_allocator:freelist_heap_test                               (cached) PASSED in 0.1s
//pw_allocator:freelist_test                                    (cached) PASSED in 0.0s
//pw_analog:analog_input_test                                   (cached) PASSED in 0.1s
//pw_analog:microvolt_input_test                                (cached) PASSED in 0.0s
//pw_arduino_build/py:builder_test                              (cached) PASSED in 0.7s
//pw_arduino_build/py:file_operations_test                      (cached) PASSED in 1.0s
//pw_assert:assert_backend_compile_test                         (cached) PASSED in 0.2s
//pw_assert:assert_facade_test                                  (cached) PASSED in 0.1s
//pw_async:fake_dispatcher_test                                 (cached) PASSED in 0.1s
//pw_async_basic:dispatcher_test                                (cached) PASSED in 0.1s
//pw_async_basic:fake_dispatcher_fixture_test                   (cached) PASSED in 0.1s
//pw_async_basic:heap_dispatcher_test                           (cached) PASSED in 0.1s
//pw_base64:base64_test                                         (cached) PASSED in 0.2s
//pw_blob_store:blob_store_chunk_write_test                     (cached) PASSED in 0.8s
//pw_blob_store:blob_store_deferred_write_test                  (cached) PASSED in 0.5s
//pw_blob_store:blob_store_test                                 (cached) PASSED in 1.0s
//pw_blob_store:flat_file_system_entry_test                     (cached) PASSED in 0.1s
//pw_bluetooth:address_test                                     (cached) PASSED in 0.3s
//pw_bluetooth:api_test                                         (cached) PASSED in 0.4s
//pw_bluetooth:result_test                                      (cached) PASSED in 0.2s
//pw_bluetooth:uuid_test                                        (cached) PASSED in 0.1s
//pw_bluetooth_hci:packet_test                                  (cached) PASSED in 0.1s
//pw_bluetooth_hci:uart_transport_fuzzer                        (cached) PASSED in 0.2s
//pw_bluetooth_hci:uart_transport_test                          (cached) PASSED in 0.1s
//pw_bluetooth_profiles:device_info_service_test                (cached) PASSED in 0.0s
//pw_build:file_prefix_map_test                                 (cached) PASSED in 0.1s
//pw_build_info:build_id_test                                   (cached) PASSED in 0.1s
//pw_bytes:array_test                                           (cached) PASSED in 0.1s
//pw_bytes:bit_test                                             (cached) PASSED in 0.1s
//pw_bytes:byte_builder_test                                    (cached) PASSED in 0.0s
//pw_bytes:endian_test                                          (cached) PASSED in 0.1s
//pw_bytes:units_test                                           (cached) PASSED in 0.1s
//pw_checksum:crc16_ccitt_test                                  (cached) PASSED in 0.1s
//pw_checksum:crc32_test                                        (cached) PASSED in 0.1s
//pw_chrono:simulated_system_clock_test                         (cached) PASSED in 0.1s
//pw_chrono:system_clock_facade_test                            (cached) PASSED in 0.1s
//pw_chrono:system_timer_facade_test                            (cached) PASSED in 0.7s
//pw_cli/py:envparse_test                                       (cached) PASSED in 0.8s
//pw_cli/py:plugins_test                                        (cached) PASSED in 0.7s
//pw_containers:algorithm_test                                  (cached) PASSED in 0.1s
//pw_containers:filtered_view_test                              (cached) PASSED in 0.1s
//pw_containers:flat_map_test                                   (cached) PASSED in 0.1s
//pw_containers:inline_deque_test                               (cached) PASSED in 0.3s
//pw_containers:inline_queue_test                               (cached) PASSED in 0.1s
//pw_containers:intrusive_list_test                             (cached) PASSED in 0.1s
//pw_containers:raw_storage_test                                (cached) PASSED in 0.1s
//pw_containers:to_array_test                                   (cached) PASSED in 0.1s
//pw_containers:vector_test                                     (cached) PASSED in 0.1s
//pw_containers:wrapped_iterator_test                           (cached) PASSED in 0.2s
//pw_cpu_exception_cortex_m/py:exception_analyzer_test          (cached) PASSED in 1.0s
//pw_crypto:ecdsa_test                                          (cached) PASSED in 0.1s
//pw_crypto:sha256_mock_test                                    (cached) PASSED in 0.1s
//pw_crypto:sha256_test                                         (cached) PASSED in 0.0s
//pw_digital_io:digital_io_test                                 (cached) PASSED in 0.2s
//pw_file:flat_file_system_test                                 (cached) PASSED in 0.2s
//pw_function:function_test                                     (cached) PASSED in 0.1s
//pw_function:pointer_test                                      (cached) PASSED in 0.3s
//pw_function:scope_guard_test                                  (cached) PASSED in 0.1s
//pw_fuzzer/examples/fuzztest:metrics_fuzztest                  (cached) PASSED in 0.1s
//pw_fuzzer/examples/fuzztest:metrics_unittest                  (cached) PASSED in 0.2s
//pw_fuzzer/examples/libfuzzer:toy_fuzzer                       (cached) PASSED in 0.2s
//pw_hdlc:decoder_test                                          (cached) PASSED in 0.3s
//pw_hdlc:encoded_size_test                                     (cached) PASSED in 0.2s
//pw_hdlc:encoder_test                                          (cached) PASSED in 0.1s
//pw_hdlc:rpc_channel_test                                      (cached) PASSED in 0.1s
//pw_hdlc:wire_packet_parser_test                               (cached) PASSED in 0.2s
//pw_hdlc/java/test/dev/pigweed/pw_hdlc:DecoderTest             (cached) PASSED in 0.5s
//pw_hdlc/java/test/dev/pigweed/pw_hdlc:EncoderTest             (cached) PASSED in 0.6s
//pw_hdlc/java/test/dev/pigweed/pw_hdlc:FrameTest               (cached) PASSED in 0.9s
//pw_hex_dump:hex_dump_test                                     (cached) PASSED in 0.1s
//pw_i2c:address_test                                           (cached) PASSED in 0.1s
//pw_i2c:device_test                                            (cached) PASSED in 0.1s
//pw_i2c:i2c_service_test                                       (cached) PASSED in 0.0s
//pw_i2c:initiator_mock_test                                    (cached) PASSED in 0.1s
//pw_i2c:register_device_test                                   (cached) PASSED in 0.1s
//pw_i2c_linux:initiator_test                                   (cached) PASSED in 0.1s
//pw_intrusive_ptr:intrusive_ptr_test                           (cached) PASSED in 0.0s
//pw_intrusive_ptr:recyclable_test                              (cached) PASSED in 0.1s
//pw_kvs:alignment_test                                         (cached) PASSED in 0.1s
//pw_kvs:checksum_test                                          (cached) PASSED in 0.2s
//pw_kvs:converts_to_span_test                                  (cached) PASSED in 0.1s
//pw_kvs:entry_cache_test                                       (cached) PASSED in 0.1s
//pw_kvs:entry_test                                             (cached) PASSED in 0.2s
//pw_kvs:fake_flash_test_key_value_store_test                   (cached) PASSED in 0.1s
//pw_kvs:flash_partition_16_alignment_test                      (cached) PASSED in 2.4s
//pw_kvs:flash_partition_1_alignment_test                       (cached) PASSED in 32.1s
//pw_kvs:flash_partition_256_alignment_test                     (cached) PASSED in 0.5s
//pw_kvs:flash_partition_256_write_size_test                    (cached) PASSED in 0.4s
//pw_kvs:flash_partition_64_alignment_test                      (cached) PASSED in 1.0s
//pw_kvs:flash_partition_stream_test                            (cached) PASSED in 0.3s
//pw_kvs:key_test                                               (cached) PASSED in 0.1s
//pw_kvs:key_value_store_16_alignment_flash_test                (cached) PASSED in 0.1s
//pw_kvs:key_value_store_1_alignment_flash_test                 (cached) PASSED in 0.3s
//pw_kvs:key_value_store_256_alignment_flash_test               (cached) PASSED in 0.1s
//pw_kvs:key_value_store_64_alignment_flash_test                (cached) PASSED in 0.2s
//pw_kvs:key_value_store_binary_format_test                     (cached) PASSED in 0.1s
//pw_kvs:key_value_store_map_test                               (cached) PASSED in 6.4s
//pw_kvs:key_value_store_put_test                               (cached) PASSED in 0.2s
//pw_kvs:key_value_store_test                                   (cached) PASSED in 0.3s
//pw_kvs:key_value_store_wear_test                              (cached) PASSED in 0.5s
//pw_kvs:sectors_test                                           (cached) PASSED in 0.1s
//pw_libc:memset_test                                           (cached) PASSED in 0.1s
//pw_log:glog_adapter_test                                      (cached) PASSED in 0.1s
//pw_log:proto_utils_test                                       (cached) PASSED in 0.0s
//pw_log:test                                                   (cached) PASSED in 0.2s
//pw_log_null:test                                              (cached) PASSED in 0.0s
//pw_log_rpc:log_filter_service_test                            (cached) PASSED in 0.1s
//pw_log_rpc:log_filter_test                                    (cached) PASSED in 0.0s
//pw_log_rpc:log_service_test                                   (cached) PASSED in 0.0s
//pw_log_rpc:rpc_log_drain_test                                 (cached) PASSED in 0.2s
//pw_log_tokenized:log_tokenized_test                           (cached) PASSED in 0.1s
//pw_log_tokenized:metadata_test                                (cached) PASSED in 0.2s
//pw_log_tokenized/py:format_string_test                        (cached) PASSED in 0.9s
//pw_log_tokenized/py:metadata_test                             (cached) PASSED in 0.6s
//pw_metric:global_test                                         (cached) PASSED in 0.6s
//pw_metric:metric_service_pwpb_test                            (cached) PASSED in 0.1s
//pw_metric:metric_test                                         (cached) PASSED in 0.2s
//pw_multisink:multisink_test                                   (cached) PASSED in 0.2s
//pw_multisink:stl_multisink_threaded_test                      (cached) PASSED in 0.1s
//pw_perf_test:chrono_timer_test                                (cached) PASSED in 0.1s
//pw_perf_test:perf_test_test                                   (cached) PASSED in 0.1s
//pw_perf_test:state_test                                       (cached) PASSED in 0.2s
//pw_perf_test:timer_test                                       (cached) PASSED in 0.1s
//pw_persistent_ram:flat_file_system_entry_test                 (cached) PASSED in 0.1s
//pw_persistent_ram:persistent_buffer_test                      (cached) PASSED in 0.3s
//pw_persistent_ram:persistent_test                             (cached) PASSED in 0.1s
//pw_polyfill:test                                              (cached) PASSED in 0.1s
//pw_preprocessor:arguments_test                                (cached) PASSED in 0.2s
//pw_preprocessor:boolean_test                                  (cached) PASSED in 0.2s
//pw_preprocessor:compiler_test                                 (cached) PASSED in 0.2s
//pw_preprocessor:concat_test                                   (cached) PASSED in 0.1s
//pw_preprocessor:util_test                                     (cached) PASSED in 0.1s
//pw_protobuf:codegen_decoder_test                              (cached) PASSED in 0.1s
//pw_protobuf:codegen_encoder_test                              (cached) PASSED in 0.2s
//pw_protobuf:decoder_fuzz_test                                 (cached) PASSED in 0.2s
//pw_protobuf:decoder_test                                      (cached) PASSED in 0.1s
//pw_protobuf:encoder_fuzz_test                                 (cached) PASSED in 0.3s
//pw_protobuf:encoder_test                                      (cached) PASSED in 0.1s
//pw_protobuf:find_test                                         (cached) PASSED in 0.1s
//pw_protobuf:map_utils_test                                    (cached) PASSED in 0.1s
//pw_protobuf:message_test                                      (cached) PASSED in 0.1s
//pw_protobuf:serialized_size_test                              (cached) PASSED in 0.2s
//pw_protobuf:stream_decoder_test                               (cached) PASSED in 0.2s
//pw_protobuf_compiler:nested_packages_test                     (cached) PASSED in 0.1s
//pw_protobuf_compiler/py:compiled_protos_test                  (cached) PASSED in 0.4s
//pw_random:xor_shift_test                                      (cached) PASSED in 0.2s
//pw_result:expected_test                                       (cached) PASSED in 0.1s
//pw_result:result_test                                         (cached) PASSED in 0.1s
//pw_result:statusor_test                                       (cached) PASSED in 0.2s
//pw_ring_buffer:prefixed_entry_ring_buffer_test                (cached) PASSED in 0.2s
//pw_router:static_router_test                                  (cached) PASSED in 0.2s
//pw_rpc:call_test                                              (cached) PASSED in 0.1s
//pw_rpc:callback_test                                          (cached) PASSED in 0.3s
//pw_rpc:channel_test                                           (cached) PASSED in 0.2s
//pw_rpc:client_server_test                                     (cached) PASSED in 0.1s
//pw_rpc:fake_channel_output_test                               (cached) PASSED in 0.1s
//pw_rpc:method_test                                            (cached) PASSED in 0.1s
//pw_rpc:packet_meta_test                                       (cached) PASSED in 0.1s
//pw_rpc:packet_test                                            (cached) PASSED in 0.0s
//pw_rpc:server_test                                            (cached) PASSED in 0.1s
//pw_rpc:service_test                                           (cached) PASSED in 0.1s
//pw_rpc:test_helpers_test                                      (cached) PASSED in 0.2s
//pw_rpc/java/test/dev/pigweed/pw_rpc:ClientTest                (cached) PASSED in 1.5s
//pw_rpc/java/test/dev/pigweed/pw_rpc:EndpointTest              (cached) PASSED in 1.5s
//pw_rpc/java/test/dev/pigweed/pw_rpc:FutureCallTest            (cached) PASSED in 1.5s
//pw_rpc/java/test/dev/pigweed/pw_rpc:IdsTest                   (cached) PASSED in 0.7s
//pw_rpc/java/test/dev/pigweed/pw_rpc:PacketsTest               (cached) PASSED in 0.6s
//pw_rpc/java/test/dev/pigweed/pw_rpc:ServiceTest               (cached) PASSED in 0.6s
//pw_rpc/java/test/dev/pigweed/pw_rpc:StreamObserverCallTest    (cached) PASSED in 1.5s
//pw_rpc/java/test/dev/pigweed/pw_rpc:StreamObserverMethodClientTest (cached) PASSED in 1.6s
//pw_rpc/nanopb:callback_test                                   (cached) PASSED in 0.3s
//pw_rpc/nanopb:client_call_test                                (cached) PASSED in 0.1s
//pw_rpc/nanopb:client_reader_writer_test                       (cached) PASSED in 0.1s
//pw_rpc/nanopb:client_server_context_test                      (cached) PASSED in 0.1s
//pw_rpc/nanopb:client_server_context_threaded_test             (cached) PASSED in 0.1s
//pw_rpc/nanopb:codegen_test                                    (cached) PASSED in 0.1s
//pw_rpc/nanopb:fake_channel_output_test                        (cached) PASSED in 0.2s
//pw_rpc/nanopb:method_info_test                                (cached) PASSED in 0.2s
//pw_rpc/nanopb:method_lookup_test                              (cached) PASSED in 0.2s
//pw_rpc/nanopb:method_test                                     (cached) PASSED in 0.1s
//pw_rpc/nanopb:method_union_test                               (cached) PASSED in 0.2s
//pw_rpc/nanopb:serde_test                                      (cached) PASSED in 0.2s
//pw_rpc/nanopb:server_callback_test                            (cached) PASSED in 0.2s
//pw_rpc/nanopb:server_reader_writer_test                       (cached) PASSED in 0.0s
//pw_rpc/nanopb:stub_generation_test                            (cached) PASSED in 0.1s
//pw_rpc/pwpb:client_call_test                                  (cached) PASSED in 0.1s
//pw_rpc/pwpb:client_reader_writer_test                         (cached) PASSED in 0.2s
//pw_rpc/pwpb:client_server_context_test                        (cached) PASSED in 0.1s
//pw_rpc/pwpb:client_server_context_threaded_test               (cached) PASSED in 0.0s
//pw_rpc/pwpb:codegen_test                                      (cached) PASSED in 0.1s
//pw_rpc/pwpb:echo_service_test                                 (cached) PASSED in 0.0s
//pw_rpc/pwpb:fake_channel_output_test                          (cached) PASSED in 0.2s
//pw_rpc/pwpb:method_info_test                                  (cached) PASSED in 0.1s
//pw_rpc/pwpb:method_lookup_test                                (cached) PASSED in 0.1s
//pw_rpc/pwpb:method_test                                       (cached) PASSED in 0.1s
//pw_rpc/pwpb:method_union_test                                 (cached) PASSED in 0.1s
//pw_rpc/pwpb:serde_test                                        (cached) PASSED in 0.1s
//pw_rpc/pwpb:server_callback_test                              (cached) PASSED in 0.2s
//pw_rpc/pwpb:server_reader_writer_test                         (cached) PASSED in 0.1s
//pw_rpc/pwpb:stub_generation_test                              (cached) PASSED in 0.2s
//pw_rpc/raw:client_reader_writer_test                          (cached) PASSED in 0.2s
//pw_rpc/raw:client_test                                        (cached) PASSED in 0.1s
//pw_rpc/raw:codegen_test                                       (cached) PASSED in 0.2s
//pw_rpc/raw:method_info_test                                   (cached) PASSED in 0.1s
//pw_rpc/raw:method_test                                        (cached) PASSED in 0.1s
//pw_rpc/raw:method_union_test                                  (cached) PASSED in 0.1s
//pw_rpc/raw:server_reader_writer_test                          (cached) PASSED in 0.2s
//pw_rpc/raw:service_nc_test                                    (cached) PASSED in 0.1s
//pw_rpc/raw:stub_generation_test                               (cached) PASSED in 0.1s
//pw_rpc_transport:egress_ingress_test                          (cached) PASSED in 0.1s
//pw_rpc_transport:hdlc_framing_test                            (cached) PASSED in 0.1s
//pw_rpc_transport:local_rpc_egress_test                        (cached) PASSED in 0.1s
//pw_rpc_transport:packet_buffer_queue_test                     (cached) PASSED in 0.1s
//pw_rpc_transport:rpc_integration_test                         (cached) PASSED in 0.2s
//pw_rpc_transport:simple_framing_test                          (cached) PASSED in 0.1s
//pw_rpc_transport:socket_rpc_transport_test                    (cached) PASSED in 0.2s
//pw_rpc_transport:stream_rpc_dispatcher_test                   (cached) PASSED in 0.1s
//pw_snapshot:cpp_compile_test                                  (cached) PASSED in 0.1s
//pw_snapshot:uuid_test                                         (cached) PASSED in 0.2s
//pw_span:compatibility_test                                    (cached) PASSED in 0.1s
//pw_span:pw_span_test                                          (cached) PASSED in 0.1s
//pw_spi:initiator_mock_test                                    (cached) PASSED in 0.2s
//pw_spi:spi_test                                               (cached) PASSED in 0.0s
//pw_status:status_test                                         (cached) PASSED in 0.2s
//pw_status:status_with_size_test                               (cached) PASSED in 0.2s
//pw_status:try_test                                            (cached) PASSED in 0.2s
//pw_status/rust:pw_status_doc_test                             (cached) PASSED in 1.0s
//pw_status/rust:pw_status_test                                 (cached) PASSED in 0.0s
//pw_stream:interval_reader_test                                (cached) PASSED in 0.2s
//pw_stream:memory_stream_test                                  (cached) PASSED in 0.1s
//pw_stream:mpsc_stream_test                                    (cached) PASSED in 0.1s
//pw_stream:null_stream_test                                    (cached) PASSED in 0.2s
//pw_stream:seek_test                                           (cached) PASSED in 0.1s
//pw_stream:socket_stream_test                                  (cached) PASSED in 0.1s
//pw_stream:std_file_stream_test                                (cached) PASSED in 0.2s
//pw_stream:stream_test                                         (cached) PASSED in 0.2s
//pw_stream/rust:pw_stream_doc_test                             (cached) PASSED in 0.8s
//pw_stream/rust:pw_stream_test                                 (cached) PASSED in 0.0s
//pw_stream_uart_linux:stream_test                              (cached) PASSED in 0.1s
//pw_string:format_test                                         (cached) PASSED in 0.1s
//pw_string:string_builder_test                                 (cached) PASSED in 0.1s
//pw_string:string_test                                         (cached) PASSED in 0.1s
//pw_string:to_string_test                                      (cached) PASSED in 0.1s
//pw_string:type_to_string_test                                 (cached) PASSED in 0.1s
//pw_string:util_test                                           (cached) PASSED in 0.1s
//pw_symbolizer/py:symbolizer_test                              (cached) PASSED in 0.6s
//pw_sync:binary_semaphore_facade_test                          (cached) PASSED in 0.3s
//pw_sync:borrow_test                                           (cached) PASSED in 0.2s
//pw_sync:counting_semaphore_facade_test                        (cached) PASSED in 0.2s
//pw_sync:inline_borrowable_test                                (cached) PASSED in 0.3s
//pw_sync:interrupt_spin_lock_facade_test                       (cached) PASSED in 0.2s
//pw_sync:lock_traits_test                                      (cached) PASSED in 0.2s
//pw_sync:mutex_facade_test                                     (cached) PASSED in 0.1s
//pw_sync:recursive_mutex_facade_test                           (cached) PASSED in 0.4s
//pw_sync:thread_notification_facade_test                       (cached) PASSED in 0.1s
//pw_sync:timed_mutex_facade_test                               (cached) PASSED in 0.1s
//pw_sync:timed_thread_notification_facade_test                 (cached) PASSED in 0.2s
//pw_thread:id_facade_test                                      (cached) PASSED in 0.2s
//pw_thread:sleep_facade_test                                   (cached) PASSED in 0.3s
//pw_thread:test_thread_context_facade_test                     (cached) PASSED in 0.2s
//pw_thread:thread_info_test                                    (cached) PASSED in 0.1s
//pw_thread:thread_snapshot_service_test                        (cached) PASSED in 0.1s
//pw_thread:yield_facade_test                                   (cached) PASSED in 0.3s
//pw_thread_stl:thread_backend_test                             (cached) PASSED in 0.2s
//pw_tokenizer:argument_types_test                              (cached) PASSED in 0.1s
//pw_tokenizer:base64_test                                      (cached) PASSED in 0.1s
//pw_tokenizer:decode_test                                      (cached) PASSED in 0.2s
//pw_tokenizer:detokenize_fuzzer                                (cached) PASSED in 0.3s
//pw_tokenizer:detokenize_test                                  (cached) PASSED in 0.2s
//pw_tokenizer:encode_args_test                                 (cached) PASSED in 0.1s
//pw_tokenizer:hash_test                                        (cached) PASSED in 0.1s
//pw_tokenizer:simple_tokenize_test                             (cached) PASSED in 0.1s
//pw_tokenizer:token_database_test                              (cached) PASSED in 0.1s
//pw_tokenizer:tokenize_test                                    (cached) PASSED in 0.1s
//pw_tokenizer/rust:pw_tokenizer_core_doc_test                  (cached) PASSED in 1.0s
//pw_tokenizer/rust:pw_tokenizer_core_test                      (cached) PASSED in 0.2s
//pw_tokenizer/rust:pw_tokenizer_doc_test                       (cached) PASSED in 0.6s
//pw_tokenizer/rust:pw_tokenizer_macro_test                     (cached) PASSED in 0.1s
//pw_tokenizer/rust:pw_tokenizer_printf_doc_test                (cached) PASSED in 1.0s
//pw_tokenizer/rust:pw_tokenizer_printf_test                    (cached) PASSED in 0.4s
//pw_tokenizer/rust:pw_tokenizer_test                           (cached) PASSED in 0.1s
//pw_toolchain:no_destructor_test                               (cached) PASSED in 0.1s
//pw_trace:trace_backend_compile_test                           (cached) PASSED in 0.1s
//pw_trace:trace_facade_test                                    (cached) PASSED in 0.1s
//pw_trace:trace_zero_facade_test                               (cached) PASSED in 0.2s
//pw_transfer:atomic_file_transfer_handler_test                 (cached) PASSED in 0.1s
//pw_transfer:chunk_test                                        (cached) PASSED in 0.1s
//pw_transfer:client_test                                       (cached) PASSED in 0.6s
//pw_transfer:handler_test                                      (cached) PASSED in 0.2s
//pw_transfer:transfer_test                                     (cached) PASSED in 0.2s
//pw_transfer:transfer_thread_test                              (cached) PASSED in 0.0s
//pw_transfer/java/test/dev/pigweed/pw_transfer:TransferClientTest (cached) PASSED in 5.4s
//pw_unit_test:framework_test                                   (cached) PASSED in 0.1s
//pw_unit_test:static_library_support_test                      (cached) PASSED in 0.1s
//pw_varint:stream_test                                         (cached) PASSED in 0.2s
//pw_varint:varint_test                                         (cached) PASSED in 0.2s
//pw_varint/rust:pw_varint_doc_test                             (cached) PASSED in 1.1s
//pw_varint/rust:pw_varint_test                                 (cached) PASSED in 0.1s
//pw_work_queue:stl_work_queue_test                             (cached) PASSED in 0.2s
//pw_cpu_exception_cortex_m:cpu_exception_entry_test                    SKIPPED
//pw_cpu_exception_cortex_m:util_test                                   SKIPPED
//pw_digital_io_mcuxpresso:mimxrt595_test                               SKIPPED
//pw_i2c_mcuxpresso:initiator_test                                      SKIPPED
//pw_spi_mcuxpresso:flexspi_test                                        SKIPPED
//pw_spi_mcuxpresso:spi_test                                            SKIPPED
//pw_stream_shmem_mcuxpresso:stream_test                                SKIPPED
//pw_stream_uart_mcuxpresso:stream_test                                 SKIPPED
//pw_console/py:command_runner_test                                      PASSED in 5.3s
//pw_console/py:console_app_test                                         PASSED in 5.0s
//pw_console/py:console_prefs_test                                       PASSED in 4.5s
//pw_console/py:help_window_test                                         PASSED in 2.8s
//pw_console/py:log_filter_test                                          PASSED in 4.2s
//pw_console/py:log_store_test                                           PASSED in 4.1s
//pw_console/py:log_view_test                                            PASSED in 4.4s
//pw_console/py:repl_pane_test                                           PASSED in 5.2s
//pw_console/py:table_test                                               PASSED in 3.0s
//pw_console/py:text_formatting_test                                     PASSED in 3.5s
//pw_console/py:window_manager_test                                      PASSED in 5.9s
//pw_hdlc/py:decode_test                                                 PASSED in 0.6s
//pw_hdlc/py:encode_test                                                 PASSED in 0.5s
//pw_hdlc/py:rpc_test                                                    PASSED in 1.5s
//pw_rpc/nanopb:synchronous_call_test                                    PASSED in 0.1s
//pw_rpc/pwpb:synchronous_call_test                                      PASSED in 0.1s
//pw_rpc/py:callback_client_test                                         PASSED in 3.2s
//pw_rpc/py:client_test                                                  PASSED in 2.1s
//pw_rpc/py:descriptors_test                                             PASSED in 1.1s
//pw_rpc/py:ids_test                                                     PASSED in 1.1s
//pw_rpc/py:packets_test                                                 PASSED in 1.2s
//pw_rpc/raw:synchronous_call_test                                       PASSED in 0.2s
//pw_tokenizer/py:decode_test                                            PASSED in 0.9s
//pw_tokenizer/py:detokenize_test                                        PASSED in 0.8s
//pw_tokenizer/py:elf_reader_test                                        PASSED in 1.1s
//pw_tokenizer/py:encode_test                                            PASSED in 0.9s
//pw_tokenizer/py:tokens_test                                            PASSED in 0.9s
//pw_transfer/integration_test:cross_language_medium_write_test          PASSED in 82.6s
//pw_transfer/integration_test:cross_language_small_test                 PASSED in 51.0s
//pw_transfer/integration_test:expected_errors_test                      PASSED in 167.6s
//pw_transfer/integration_test:legacy_binaries_test                      PASSED in 53.2s
//pw_transfer/integration_test:multi_transfer_test                       PASSED in 13.1s
//pw_transfer/integration_test:proxy_test                                PASSED in 2.4s
//pw_transfer/py:transfer_test                                           PASSED in 1.4s
//pw_transfer/integration_test:cross_language_medium_read_test           FAILED in 82.1s
/b/s/w/ir/cache/bazel/_bazel_swarming/823a25200ba977576a072121498f22ec/execroot/pigweed/bazel-out/k8-fastbuild/testlogs/pw_transfer/integration_test/cross_language_medium_read_test/test.log

Executed 35 out of 343 tests: 334 tests pass, 1 fails locally and 8 were skipped.
There were tests whose specified size is too big. Use the --test_verbose_timeout_warnings command line option to see which ones these are.
"""

_REAL_TEST_SUMMARY_2 = """
FAIL: //pw_transfer/integration_test:cross_language_medium_read_test (see /b/s/w/ir/cache/bazel/_bazel_swarming/823a25200ba977576a072121498f22ec/execroot/pigweed/bazel-out/k8-fastbuild/testlogs/pw_transfer/integration_test/cross_language_medium_read_test/test.log)
INFO: From Testing //pw_transfer/integration_test:cross_language_medium_read_test:
stdout (/b/s/w/ir/cache/bazel/_bazel_swarming/823a25200ba977576a072121498f22ec/execroot/pigweed/bazel-out/_tmp/actions/stdout-2115) 1131999 exceeds maximum size of --experimental_ui_max_stdouterr_bytes=1048576 bytes; skipping
INFO: Elapsed time: 507.459s, Critical Path: 181.66s
INFO: 1206 processes: 1235 linux-sandbox, 4 worker.
INFO: Build completed, 1 test FAILED, 1206 total actions
//pw_transfer/integration_test:cross_language_medium_read_test           FAILED in 82.1s
/b/s/w/ir/cache/bazel/_bazel_swarming/823a25200ba977576a072121498f22ec/execroot/pigweed/bazel-out/k8-fastbuild/testlogs/pw_transfer/integration_test/cross_language_medium_read_test/test.log
Executed 35 out of 343 tests: 334 tests pass, 1 fails locally and 8 were skipped.
"""

# pylint: disable=attribute-defined-outside-init


class TestBazelParser(unittest.TestCase):
    """Test bazel_parser."""

    def _run(self, contents: str) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            path = Path(tempdir) / 'foo'

            with path.open('w') as outs:
                outs.write(contents)

            self.output = bazel_parser.parse_bazel_stdout(path)

    def test_simple(self) -> None:
        error = 'ERROR: abc\nerror 1\nerror2\n'
        self._run('[0/10] foo\n[1/10] bar\n' + error)
        self.assertEqual(error.strip(), self.output.strip())

    def test_path(self) -> None:
        error_in = 'ERROR: abc\n PATH=... \\\nerror 1\nerror2\n'
        error_out = 'ERROR: abc\nerror 1\nerror2\n'
        self._run('[0/10] foo\n[1/10] bar\n' + error_in)
        self.assertEqual(error_out.strip(), self.output.strip())

    def test_failure(self) -> None:
        self._run(_REAL_TEST_INPUT)
        self.assertEqual(_REAL_TEST_SUMMARY.strip(), self.output.strip())

    def test_failure_2(self) -> None:
        self._run(_REAL_TEST_INPUT_2)
        self.assertEqual(_REAL_TEST_SUMMARY_2.strip(), self.output.strip())


if __name__ == '__main__':
    unittest.main()
