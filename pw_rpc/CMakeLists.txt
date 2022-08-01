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

if(NOT "${dir_pw_third_party_nanopb}" STREQUAL "")
  add_subdirectory(nanopb)
endif()

add_subdirectory(pwpb)
add_subdirectory(raw)
add_subdirectory(system_server)

pw_add_module_config(pw_rpc_CONFIG)

pw_add_module_library(pw_rpc.config
  HEADERS
    public/pw_rpc/internal/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_rpc_CONFIG}
)

pw_add_module_library(pw_rpc.server
  SOURCES
    call.cc
    endpoint.cc
    server.cc
    server_call.cc
    service.cc
  PUBLIC_DEPS
    pw_rpc.common
  PRIVATE_DEPS
    pw_log
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_SERVER)
  zephyr_link_libraries(pw_rpc.server)
endif()

pw_add_module_library(pw_rpc.client
  SOURCES
    client.cc
    client_call.cc
  PUBLIC_DEPS
    pw_rpc.common
    pw_result
  PRIVATE_DEPS
    pw_log
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_CLIENT)
  zephyr_link_libraries(pw_rpc.client)
endif()

pw_add_module_library(pw_rpc.client_server
  SOURCES
    client_server.cc
  PUBLIC_DEPS
    pw_rpc.client
    pw_rpc.server
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_CLIENT_SERVER)
  zephyr_link_libraries(pw_rpc.client_server)
endif()

pw_add_module_library(pw_rpc.synchronous_client_api
  HEADERS
    public/pw_rpc/synchronous_call.h
    public/pw_rpc/synchronous_call_result.h
  PUBLIC_DEPS
    pw_chono.system_clock
    pw_rpc.client
    pw_rpc.common
    pw_sync.timed_thread_notification
)

pw_add_module_library(pw_rpc.common
  HEADERS
    public/pw_rpc/method_info.h
    public/pw_rpc/packet_meta.h
    public/pw_rpc/service_id.h
  SOURCES
    channel.cc
    channel_list.cc
    packet.cc
    packet_meta.cc
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_containers
    pw_function
    pw_rpc.config
    pw_rpc.protos.pwpb
    pw_span
    pw_status
    pw_sync.lock_annotations
  PRIVATE_DEPS
    pw_log
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_RPC_COMMON)
  zephyr_link_libraries(pw_rpc.common)
endif()

if (NOT "${pw_sync.mutex_BACKEND}" STREQUAL "pw_sync.mutex.NO_BACKEND_SET" AND
    NOT "${pw_sync.mutex_BACKEND}" STREQUAL "")
  target_link_libraries(pw_rpc.common PUBLIC pw_sync.mutex)
endif()

pw_add_module_library(pw_rpc.test_utils
  SOURCES
    fake_channel_output.cc
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_rpc.client
    pw_rpc.server
  PRIVATE_DEPS
    pw_log
)
target_include_directories(pw_rpc.test_utils PUBLIC .)

pw_add_module_library(pw_rpc.client_server_testing
  PUBLIC_DEPS
    pw_bytes
    pw_result
    pw_rpc.client_server
    pw_rpc.test_utils
)

pw_add_module_library(pw_rpc.client_server_testing_threaded
  PUBLIC_DEPS
    pw_bytes
    pw_result
    pw_rpc.client_server_testing
    pw_sync.binary_semaphore
    pw_sync.lock_annotations
    pw_sync.mutex
    pw_thread.thread
)

pw_proto_library(pw_rpc.protos
  SOURCES
    internal/packet.proto
    echo.proto
  INPUTS
    echo.options
  PREFIX
    pw_rpc
)

pw_proto_library(pw_rpc.test_protos
  SOURCES
    pw_rpc_test_protos/no_package.proto
    pw_rpc_test_protos/test.proto
)

# Set pw_rpc_CONFIG to this to disable the global mutex.
add_library(pw_rpc.disable_global_mutex_config INTERFACE)
target_compile_definitions(pw_rpc.disable_global_mutex_config
  INTERFACE
    PW_RPC_USE_GLOBAL_MUTEX=0
)

pw_auto_add_module_tests(pw_rpc
  PRIVATE_DEPS
    pw_rpc.client
    pw_rpc.server
)
