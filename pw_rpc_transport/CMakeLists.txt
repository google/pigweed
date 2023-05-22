# Copyright 2023 The Pigweed Authors
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

pw_add_library(pw_rpc_transport.rpc_transport INTERFACE
  HEADERS
    public/pw_rpc_transport/rpc_transport.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_function
    pw_status
)

pw_add_library(pw_rpc_transport.service_registry INTERFACE
  HEADERS
    public/pw_rpc_transport/service_registry.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_rpc_transport.rpc_transport
    pw_rpc.client_server
    pw_span
    pw_status
)

pw_add_library(pw_rpc_transport.packet_buffer_queue INTERFACE
  HEADERS
    public/pw_rpc_transport/internal/packet_buffer_queue.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_containers
    pw_result
    pw_status
    pw_sync.lock_annotations
    pw_sync.mutex
)

pw_add_test(pw_rpc_transport.packet_buffer_queue_test
  SOURCES
    internal/packet_buffer_queue_test.cc
  PRIVATE_DEPS
    pw_rpc_transport.packet_buffer_queue
    pw_bytes
    pw_containers
    pw_result
    pw_status
    pw_sync.lock_annotations
    pw_sync.mutex
  GROUPS
    modules
    pw_rpc_transport
)

pw_add_library(pw_rpc_transport.local_rpc_egress STATIC
  HEADERS
    public/pw_rpc_transport/local_rpc_egress.h
  SOURCES
    local_rpc_egress.cc
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_rpc_transport.packet_buffer_queue
    pw_rpc_transport.rpc_transport
    pw_bytes
    pw_result
    pw_rpc.client
    pw_status
    pw_sync.thread_notification
    pw_thread.thread_core
  PRIVATE_DEPS
    pw_log
)

pw_add_test(pw_rpc_transport.local_rpc_egress_test
  SOURCES
    local_rpc_egress_test.cc
  PRIVATE_DEPS
    pw_rpc_transport.local_rpc_egress
    pw_rpc_transport.rpc_transport
    pw_rpc_transport.service_registry
    pw_rpc_transport.test_protos.pwpb_rpc
    pw_bytes
    pw_chrono.system_clock
    pw_rpc.client_server
    pw_status
    pw_sync.counting_semaphore
    pw_sync.thread_notification
    pw_thread.sleep
    pw_thread.thread
  GROUPS
    modules
    pw_rpc_transport
)

pw_add_library(pw_rpc_transport.hdlc_framing INTERFACE
  HEADERS
    public/pw_rpc_transport/hdlc_framing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_rpc_transport.rpc_transport
    pw_bytes
    pw_hdlc.decoder
    pw_hdlc.encoder
    pw_result
    pw_status
    pw_stream
)

pw_add_test(pw_rpc_transport.hdlc_framing_test
  SOURCES
    hdlc_framing_test.cc
  PRIVATE_DEPS
    pw_rpc_transport.hdlc_framing
    pw_bytes
    pw_status
  GROUPS
    modules
    pw_rpc_transport
)

pw_add_library(pw_rpc_transport.simple_framing STATIC
  HEADERS
    public/pw_rpc_transport/simple_framing.h
  SOURCES
    simple_framing.cc
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_rpc_transport.rpc_transport
    pw_assert
    pw_bytes
    pw_status
  PRIVATE_DEPS
    pw_log
)

pw_add_test(pw_rpc_transport.simple_framing_test
  SOURCES
    simple_framing_test.cc
  PRIVATE_DEPS
    pw_rpc_transport.simple_framing
    pw_bytes
    pw_log
    pw_status
  GROUPS
    modules
    pw_rpc_transport
)

pw_add_library(pw_rpc_transport.egress_ingress STATIC
  HEADERS
    public/pw_rpc_transport/egress_ingress.h
  SOURCES
    egress_ingress.cc
  PUBLIC_DEPS
    pw_rpc_transport.hdlc_framing
    pw_rpc_transport.simple_framing
    pw_bytes
    pw_metric
    pw_rpc.client
    pw_status
  PRIVATE_DEPS
    pw_log
)

pw_add_test(pw_rpc_transport.egress_ingress_test
  SOURCES
    egress_ingress_test.cc
  PRIVATE_DEPS
    pw_rpc_transport.egress_ingress
    pw_rpc_transport.service_registry
    pw_rpc_transport.test_protos.pwpb_rpc
    pw_bytes
    pw_metric
    pw_status
    pw_sync.thread_notification
  GROUPS
    modules
    pw_rpc_transport
)

pw_proto_library(pw_rpc_transport.test_protos
  SOURCES
    internal/test.proto
  INPUTS
    internal/test.options
  PREFIX
    pw_rpc_transport
)
