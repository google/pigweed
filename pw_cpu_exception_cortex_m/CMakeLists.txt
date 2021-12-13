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

pw_add_module_library(pw_cpu_exception_cortex_m.cpu_exception_armv7m
  IMPLEMENTS_FACADES
    pw_cpu_exception.entry
  PUBLIC_DEPS
    pw_preprocessor
  PRIVATE_DEPS
    pw_cpu_exception.handler
    pw_cpu_exception_cortex_m.constants_armv7m
  SOURCES
    entry.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/cpu_state.h
    public_overrides/pw_cpu_exception_backend/state.h
  PUBLIC_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV7M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.cpu_exception_armv8m
  IMPLEMENTS_FACADES
    pw_cpu_exception.entry
  PUBLIC_DEPS
    pw_preprocessor
  PRIVATE_DEPS
    pw_cpu_exception.handler
    pw_cpu_exception_cortex_m.constants_armv8m
  SOURCES
    entry.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/cpu_state.h
    public_overrides/pw_cpu_exception_backend/state.h
  PUBLIC_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV8M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.support_armv7m
  IMPLEMENTS_FACADES
    pw_cpu_exception.support
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.constants_armv7m
    pw_log
    pw_string
  SOURCES
    cpu_state.cc
  PRIVATE_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV7M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.support_armv8m
  IMPLEMENTS_FACADES
    pw_cpu_exception.support
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.constants_armv8m
    pw_log
    pw_string
  SOURCES
    cpu_state.cc
  PRIVATE_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV8M=1"
)

pw_proto_library(pw_cpu_exception_cortex_m.cpu_state_protos
  SOURCES
    pw_cpu_exception_cortex_m_protos/cpu_state.proto
)

pw_add_module_library(pw_cpu_exception_cortex_m.proto_dump_armv7m
  PUBLIC_DEPS
    pw_cpu_exception.entry
    pw_protobuf
    pw_status
    pw_stream
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.cpu_state_protos.pwpb
  SOURCES
    proto_dump.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/proto_dump.h
  PRIVATE_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV7M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.proto_dump_armv8m
  PUBLIC_DEPS
    pw_cpu_exception.entry
    pw_protobuf
    pw_status
    pw_stream
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.cpu_state_protos.pwpb
  SOURCES
    proto_dump.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/proto_dump.h
  PRIVATE_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV8M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.snapshot_armv7m
  PUBLIC_DEPS
    pw_cpu_exception_cortex_m.cpu_state_protos.pwpb
    pw_cpu_exception_cortex_m.cpu_exception_armv7m
    pw_protobuf
    pw_status
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.constants_armv7m
    pw_cpu_exception_cortex_m.proto_dump_armv7m
    pw_log
  SOURCES
    snapshot.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/snapshot.h
  PRIVATE_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV7M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.snapshot_armv8m
  PUBLIC_DEPS
    pw_cpu_exception_cortex_m.cpu_state_protos.pwpb
    pw_cpu_exception_cortex_m.cpu_exception_armv8m
    pw_protobuf
    pw_status
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.constants_armv8m
    pw_cpu_exception_cortex_m.proto_dump_armv8m
    pw_log
  SOURCES
    snapshot.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/snapshot.h
  PRIVATE_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV8M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.constants_armv7m
  HEADERS
    pw_cpu_exception_cortex_m_private/cortex_m_constants.h
  PUBLIC_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV7M=1"
)

pw_add_module_library(pw_cpu_exception_cortex_m.constants_armv8m
  HEADERS
    pw_cpu_exception_cortex_m_private/cortex_m_constants.h
  PUBLIC_DEFINES
    "PW_CPU_EXCEPTION_CORTEX_M_ARMV8M=1"
)
