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

pw_add_module_library(pw_trace_tokenized
  IMPLEMENTS_FACADES
    pw_trace
  SOURCES
    trace.cc
  PRIVATE_DEPS
    pw_assert
    pw_log
    pw_ring_buffer
    pw_varint
  PUBLIC_DEPS
    pw_status
    pw_tokenizer
)

pw_add_module_library(pw_trace_tokenized.trace_buffer
  SOURCES
    trace_buffer.cc
  PRIVATE_DEPS
    pw_assert
    pw_bytes
    pw_log
  PUBLIC_DEPS
    pw_ring_buffer
    pw_status
    pw_tokenizer
    pw_trace_tokenized
    pw_varint
)

pw_proto_library(pw_trace_tokenized.protos
  SOURCES
    pw_trace_protos/trace_rpc.proto
  INPUTS
    pw_trace_protos/trace_rpc.options
)

pw_add_module_library(pw_trace_tokenized.rpc_service
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
