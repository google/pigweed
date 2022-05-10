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

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_module_library(pw_rpc.nanopb.method
  SOURCES
    method.cc
    server_reader_writer.cc
  PUBLIC_DEPS
    pw_rpc.nanopb.common
    pw_rpc.server
  PRIVATE_DEPS
    pw_log
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_NANOPB_METHOD)
  zephyr_link_libraries(pw_rpc.nanopb.method)
endif()

pw_add_module_library(pw_rpc.nanopb.method_union
  PUBLIC_DEPS
    pw_rpc.nanopb.method
    pw_rpc.raw
    pw_rpc.server
  PRIVATE_DEPS
    pw_log
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_NANOPB_METHOD_UNION)
  zephyr_link_libraries(pw_rpc.nanopb.method_union)
endif()

pw_add_module_library(pw_rpc.nanopb.client
  PUBLIC_DEPS
    pw_function
    pw_rpc.nanopb.common
    pw_rpc.common
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_NANOPB_CLIENT)
  zephyr_link_libraries(pw_rpc.nanopb.client)
endif()

pw_add_module_library(pw_rpc.nanopb.common
  SOURCES
    common.cc
  PUBLIC_DEPS
    pw_bytes
    pw_log
    pw_rpc.common
    pw_third_party.nanopb
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_NANOPB_COMMON)
  zephyr_link_libraries(pw_rpc.nanopb.common)
endif()

pw_add_module_library(pw_rpc.nanopb.echo_service
  PUBLIC_DEPS
    pw_rpc.protos.nanopb_rpc
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_NANOPB_ECHO_SERVICE)
  zephyr_link_libraries(pw_rpc.nanopb.echo_service)
endif()

pw_add_module_library(pw_rpc.nanopb.client_testing
  PUBLIC_DEPS
    pw_rpc.client
    pw_rpc.raw
    pw_rpc.server
    pw_rpc.nanopb.common
    pw_rpc.protos.nanopb_rpc
    pw_rpc.test_protos.nanopb_rpc
    pw_rpc.test_utils
)

pw_add_module_library(pw_rpc.nanopb.client_server_testing
  PUBLIC_DEPS
    pw_rpc.nanopb.client_testing
    pw_rpc.client_server_testing
)

pw_add_module_library(pw_rpc.nanopb.client_server_testing_threaded
  PUBLIC_DEPS
    pw_rpc.nanopb.client_testing
    pw_rpc.client_server_testing_threaded
)

pw_add_test(pw_rpc.nanopb.client_call_test
  SOURCES
    client_call_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.client_reader_writer_test
  SOURCES
    client_reader_writer_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.client_server_context_test
  SOURCES
    client_server_context_test.cc
  DEPS
    pw_rpc.nanopb.client_server_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

if(("${pw_thread.thread_BACKEND}" STREQUAL "pw_thread_stl.thread") AND
   (NOT "${pw_sync.binary_semaphore_BACKEND}" STREQUAL
   "pw_sync.binary_semaphore.NO_BACKEND_SET") AND
   (NOT "${pw_sync.mutex_BACKEND}" STREQUAL
   "pw_sync.binary_semaphore.NO_BACKEND_SET"))
  pw_add_test(pw_rpc.nanopb.client_server_context_threaded_test
    SOURCES
      client_server_context_threaded_test.cc
    DEPS
      pw_rpc.nanopb.client_server_testing_threaded
      pw_sync.binary_semaphore
      pw_thread.test_threads
      pw_thread_stl.test_threads
      pw_thread.thread
    GROUPS
      modules
      pw_rpc.nanopb
  )
endif()

pw_add_test(pw_rpc.nanopb.codegen_test
  SOURCES
    codegen_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.fake_channel_output_test
  SOURCES
    fake_channel_output_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.method_test
  SOURCES
    method_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.method_info_test
  SOURCES
    method_info_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.method_lookup_test
  SOURCES
    method_lookup_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.method_union_test
  SOURCES
    method_union_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.echo_service_test
  SOURCES
    echo_service_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.serde_test
  SOURCES
    serde_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.server_callback_test
  SOURCES
    server_callback_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.server_reader_writer_test
  SOURCES
    server_reader_writer_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

pw_add_test(pw_rpc.nanopb.stub_generation_test
  SOURCES
    stub_generation_test.cc
  DEPS
    pw_rpc.nanopb.client_testing
  GROUPS
    modules
    pw_rpc.nanopb
)

# TODO(b/231950909) Test disabled as pw_work_queue lacks CMakeLists.txt
if((TARGET pw_work_queue.pw_work_queue) AND
   ("${pw_thread.thread_BACKEND}" STREQUAL "pw_thread_stl.thread") AND
   (NOT "${pw_sync.timed_thread_notification_BACKEND}" STREQUAL
   "pw_sync.timed_thread_notification.NO_BACKEND_SET"))
  pw_add_test(pw_rpc.nanopb.synchronous_call_test
    SOURCES
      synchronous_call_test.cc
    DEPS
      pw_rpc.nanopb.client_testing
      pw_rpc.synchronous_client_api
      pw_thread.thread
      pw_work_queue.pw_work_queue
      pw_work_queue.test_thread
      pw_work_queue.stl_test_thread
    GROUPS
      modules
      pw_rpc.nanopb
  )
endif()
