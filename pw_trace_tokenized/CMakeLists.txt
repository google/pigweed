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
include($ENV{PW_ROOT}/pw_trace_tokenized/backend.cmake)
include($ENV{PW_ROOT}/pw_trace_tokenized/lock.cmake)

pw_add_module_config(pw_trace_tokenized_CONFIG)

pw_add_library(pw_trace_tokenized.config INTERFACE
  HEADERS
    public/pw_trace_tokenized/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_trace_CONFIG}
)

pw_add_library(pw_trace_tokenized.fake_trace_time STATIC
  PRIVATE_DEPS
    pw_trace_tokenized.config
  SOURCES
    fake_trace_time.cc
)

pw_add_library(pw_trace_tokenized.host_trace_time STATIC
  PRIVATE_DEPS
    pw_trace_tokenized.config
  SOURCES
    host_trace_time.cc
)

if(CONFIG_ZEPHYR_PIGWEED_MODULE)
  pw_add_library(pw_trace_tokenized.zephyr_trace_time STATIC
    PRIVATE_DEPS
      pw_trace_tokenized.config
    PUBLIC_DEPS
      zephyr_interface
    SOURCES
      zephyr_trace_time.cc
  )
  if(CONFIG_PIGWEED_TRACE_TOKENIZED_TIME)
    set(pw_trace_tokenizer_time pw_trace_tokenized.zephyr_trace_time CACHE STRING "Tokenizer trace time implementation" FORCE)
  endif()
endif()

set(pw_trace_tokenized_LOCK_BACKEND pw_trace_tokenized.interrupt_spin_lock)

pw_add_library(pw_trace_tokenized.interrupt_spin_lock INTERFACE
  HEADERS
    public/pw_trace_tokenized/internal/interrupt_spin_lock_native.h
    interrupt_spin_lock_public_overrides/pw_trace_tokenized_backend/lock_native.h
  PUBLIC_INCLUDES
    public
    interrupt_spin_lock_public_overrides
  PUBLIC_DEPS
    pw_sync.interrupt_spin_lock
)

pw_add_library(pw_trace_tokenized.no_lock INTERFACE
  HEADERS
    public/pw_trace_tokenized/internal/no_lock_native.h
    no_lock_public_overrides/pw_trace_tokenized_backend/lock_native.h
  PUBLIC_INCLUDES
    public
    no_lock_public_overrides
  PUBLIC_DEPS
    pw_sync.no_lock
)

pw_add_facade(pw_trace_tokenized.lock INTERFACE
  BACKEND
    pw_trace_tokenized_LOCK_BACKEND
  HEADERS
    public/pw_trace_tokenized/internal/lock.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_trace_tokenized.core STATIC
  HEADERS
    public/pw_trace_tokenized/internal/trace_tokenized_internal.h
    public/pw_trace_tokenized/trace_callback.h
    public/pw_trace_tokenized/trace_tokenized.h
    public_overrides/pw_trace_backend/trace_backend.h
  PUBLIC_INCLUDES
    public
    public_overrides
  PUBLIC_DEPS
    pw_log
    pw_span
    pw_status
    pw_tokenizer
  SOURCES
    trace.cc
  PRIVATE_DEPS
    pw_trace_tokenized.config
    pw_trace_tokenized.lock
    pw_assert
    pw_ring_buffer
    pw_trace.facade
    pw_varint
    ${pw_trace_tokenizer_time}
)

pw_add_library(pw_trace_tokenized INTERFACE
  PUBLIC_DEPS
    pw_trace_tokenized.core
    pw_trace_tokenized.config
  PUBLIC_INCLUDES
    public
    public_overrides
  PRIVATE_DEPS
    ${pw_trace_tokenizer_time}
)

if(("${pw_trace_BACKEND}" STREQUAL "pw_trace_tokenized") AND
   (NOT "${pw_trace_tokenizer_time}" STREQUAL ""))
pw_add_test(pw_trace_tokenized.trace_tokenized_test
  SOURCES
    trace_test.cc
  PRIVATE_DEPS
    pw_thread.sleep
    pw_trace_tokenized.core
    pw_trace
    ${pw_trace_tokenizer_time}
  GROUPS
    modules
    pw_trace_tokenized
)
endif()

pw_add_library(pw_trace_tokenized.trace_buffer STATIC
  HEADERS
    public/pw_trace_tokenized/trace_buffer.h
  PUBLIC_INCLUDES
    public
  SOURCES
    trace_buffer.cc
  PRIVATE_DEPS
    pw_trace_tokenized.core
  PUBLIC_DEPS
    pw_bytes
    pw_ring_buffer
    pw_tokenizer
    pw_trace_tokenized.config
    pw_varint
    pw_span
  PUBLIC_DEFINES
    PW_TRACE_BUFFER_SIZE_BYTES=${pw_trace_tokenized_BUFFER_SIZE}
)

pw_proto_library(pw_trace_tokenized.protos
  SOURCES
    pw_trace_protos/trace_rpc.proto
    pw_trace_protos/trace_service.proto
  INPUTS
    pw_trace_protos/trace_rpc.options
    pw_trace_protos/trace_service.options
  DEPS
    pw_chrono.protos
)

pw_add_library(pw_trace_tokenized.rpc_service STATIC
  SOURCES
    trace_rpc_service_nanopb.cc
  PRIVATE_DEPS
    pw_assert
    pw_log
    pw_ring_buffer
    pw_trace_tokenized.trace_buffer
    pw_trace_tokenized.protos.nanopb_rpc
    pw_varint
  PUBLIC_DEPS
    pw_tokenizer
    pw_status
)

pw_add_library(pw_trace_tokenized.base_trace_service STATIC
  HEADERS
    public/pw_trace_tokenized/base_trace_service.h
  SOURCES
    base_trace_service.cc
  PUBLIC_INCLUDES
    public
  PRIVATE_DEPS
    pw_log
    pw_stream
    pw_ring_buffer
  PUBLIC_DEPS
    pw_trace_tokenized
    pw_trace_tokenized.trace_buffer
)

pw_add_library(pw_trace_tokenized.trace_service_pwpb STATIC
  SOURCES
    trace_service_pwpb.cc
  PUBLIC_DEPS
    pw_trace_tokenized.base_trace_service
    pw_trace_tokenized.protos.pwpb_rpc
)

pw_add_library(pw_trace_tokenized.transfer_handler STATIC
  HEADERS
    public/pw_trace_tokenized/transfer_handler.h
  SOURCES
    transfer_handler.cc
  PRIVATE_DEPS
    pw_log
    pw_ring_buffer
    pw_tokenizer
    pw_trace_tokenized.trace_buffer
  PUBLIC_DEPS
    pw_transfer
)
