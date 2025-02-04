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
include($ENV{PW_ROOT}/pw_protobuf_compiler/proto.cmake)

pw_add_module_config(pw_transfer_CONFIG)

pw_add_library(pw_transfer.config INTERFACE
  PUBLIC_DEPS
    ${pw_transfer_CONFIG}
    pw_chrono.system_timer
  HEADERS
    public/pw_transfer/internal/config.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_transfer STATIC
  HEADERS
    public/pw_transfer/handler.h
    public/pw_transfer/transfer.h
    public/pw_transfer/internal/chunk.h
    public/pw_transfer/internal/event.h
  PUBLIC_INCLUDES
    public
  SOURCES
    transfer.cc
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_result
    pw_status
    pw_stream
    pw_sync.mutex
    pw_transfer.core
    pw_transfer.proto.raw_rpc
  PRIVATE_DEPS
    pw_log
    pw_log.rate_limited
    pw_transfer.proto.pwpb
)

pw_add_library(pw_transfer.client STATIC
  HEADERS
    public/pw_transfer/client.h
  PUBLIC_INCLUDES
    public
  SOURCES
    client.cc
  PUBLIC_DEPS
    pw_assert
    pw_function
    pw_stream
    pw_transfer.core
    pw_transfer.proto.raw_rpc
  PRIVATE_DEPS
    pw_log
)

pw_add_library(pw_transfer.core STATIC
  PUBLIC_DEPS
    pw_bytes
    pw_chrono.system_clock
    pw_containers.intrusive_list
    pw_result
    pw_rpc.client
    pw_rpc.raw.client_api
    pw_rpc.raw.server_api
    pw_status
    pw_stream
    pw_sync.binary_semaphore
    pw_sync.timed_thread_notification
    pw_thread.thread_core
    pw_transfer.config
    pw_transfer.proto.pwpb
  HEADERS
    public/pw_transfer/internal/chunk.h
    public/pw_transfer/internal/client_context.h
    public/pw_transfer/internal/context.h
    public/pw_transfer/internal/event.h
    public/pw_transfer/internal/protocol.h
    public/pw_transfer/internal/server_context.h
  SOURCES
    chunk.cc
    client_context.cc
    context.cc
    rate_estimate.cc
    server_context.cc
    transfer_thread.cc
  PRIVATE_DEPS
    pw_log
    pw_log.rate_limited
    pw_protobuf
    pw_varint
)

pw_proto_library(pw_transfer.proto
  SOURCES
    transfer.proto
  PREFIX
    pw_transfer
)

pw_add_library(pw_transfer.test_helpers INTERFACE
  HEADERS
    pw_transfer_private/chunk_testing.h
  PUBLIC_DEPS
    pw_transfer.core
    pw_containers
)

pw_add_library(pw_transfer.atomic_file_transfer_handler STATIC
  PUBLIC_INCLUDES
    public
  HEADERS
    public/pw_transfer/atomic_file_transfer_handler.h
  SOURCES
    atomic_file_transfer_handler.cc
  PUBLIC_DEPS
    pw_transfer.core
    pw_stream.std_file_stream
  PRIVATE_DEPS
    pw_transfer.atomic_file_transfer_handler_internal
    pw_log
)

pw_add_library(pw_transfer.atomic_file_transfer_handler_internal INTERFACE
  HEADERS
    pw_transfer_private/filename_generator.h
)

if("${pw_thread.thread_BACKEND}" STREQUAL "pw_thread_stl.thread")
  pw_add_test(pw_transfer.client_test
    SOURCES
      client_test.cc
    PRIVATE_DEPS
      pw_rpc.raw.client_testing
      pw_rpc.test_helpers
      pw_thread.sleep
      pw_thread.thread
      pw_transfer.client
      pw_transfer.test_helpers
    GROUPS
      modules
      pw_transfer
  )
endif()

if(("${pw_thread.thread_BACKEND}" STREQUAL "pw_thread_stl.thread") AND
   (${pw_unit_test_BACKEND} STREQUAL "pw_unit_test.light"))
  pw_add_test(pw_transfer.transfer_thread_test
    SOURCES
      transfer_thread_test.cc
    PRIVATE_DEPS
      pw_transfer.proto.raw_rpc
      pw_transfer.core
      pw_transfer
      pw_transfer.test_helpers
      pw_rpc.test_helpers
      pw_rpc.raw.client_testing
      pw_rpc.raw.test_method_context
      pw_thread.thread
    GROUPS
      modules
      pw_transfer
  )
endif()

if(("${pw_thread.thread_BACKEND}" STREQUAL "pw_thread_stl.thread") AND
   (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows"))
  pw_add_test(pw_transfer.chunk_test
    SOURCES
      chunk_test.cc
    PRIVATE_DEPS
      pw_transfer.core
    GROUPS
      modules
      pw_transfer
  )

  pw_add_test(pw_transfer.handler_test
    SOURCES
      handler_test.cc
    PRIVATE_DEPS
      pw_transfer
    GROUPS
      modules
      pw_transfer
  )

  pw_add_test(pw_transfer.atomic_file_transfer_handler_test
    SOURCES
      atomic_file_transfer_handler_test.cc
    PRIVATE_DEPS
      pw_transfer.atomic_file_transfer_handler
      pw_transfer.atomic_file_transfer_handler_internal
      pw_transfer
      pw_random
      pw_string
    GROUPS
      modules
      pw_transfer
  )
endif()
