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

pw_add_module_config(pw_cpu_exception_cortex_m_CONFIG)

pw_add_module_library(pw_cpu_exception_cortex_m.config
  HEADERS
    pw_cpu_exception_cortex_m_private/config.h
  PUBLIC_DEPS
    ${pw_cpu_exception_cortex_m_CONFIG}
)

pw_add_module_library(pw_cpu_exception_cortex_m.cpu_state
  HEADERS
    public/pw_cpu_exception_cortex_m/cpu_state.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_preprocessor
    pw_preprocessor.arch
)

pw_add_module_library(pw_cpu_exception_cortex_m.cpu_exception
  IMPLEMENTS_FACADES
    pw_cpu_exception.entry
  HEADERS
    public_overrides/pw_cpu_exception_backend/state.h
  PUBLIC_INCLUDES
    public_overrides
  PUBLIC_DEPS
    pw_preprocessor
    pw_preprocessor.arch
    pw_cpu_exception_cortex_m.cpu_state
  PRIVATE_DEPS
    pw_cpu_exception.handler
    pw_cpu_exception_cortex_m.config
    pw_cpu_exception_cortex_m.constants
    pw_cpu_exception_cortex_m.util
  SOURCES
    entry.cc
)

pw_add_module_library(pw_cpu_exception_cortex_m.util
  HEADERS
    public/pw_cpu_exception_cortex_m/util.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_cpu_exception_cortex_m.cpu_state
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.config
    pw_cpu_exception_cortex_m.constants
    pw_log
    pw_preprocessor.arch
  SOURCES
    util.cc
)

pw_add_module_library(pw_cpu_exception_cortex_m.support
  IMPLEMENTS_FACADES
    pw_cpu_exception.support
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.config
    pw_cpu_exception_cortex_m.constants
    pw_cpu_exception_cortex_m.util
    pw_log
    pw_polyfill.span
    pw_preprocessor.arch
    pw_string
  SOURCES
    support.cc
)

pw_proto_library(pw_cpu_exception_cortex_m.cpu_state_protos
  SOURCES
    pw_cpu_exception_cortex_m_protos/cpu_state.proto
)

pw_add_module_library(pw_cpu_exception_cortex_m.proto_dump
  HEADERS
    public/pw_cpu_exception_cortex_m/proto_dump.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_cpu_exception_cortex_m.cpu_state
    pw_protobuf
    pw_status
    pw_stream
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.config
    pw_cpu_exception_cortex_m.cpu_state_protos.pwpb
  SOURCES
    proto_dump.cc
)

pw_add_module_library(pw_cpu_exception_cortex_m.snapshot
  PUBLIC_DEPS
    pw_cpu_exception_cortex_m.cpu_state
    pw_cpu_exception_cortex_m.cpu_state_protos.pwpb
    pw_protobuf
    pw_status
    pw_thread.protos.pwpb
    pw_thread.snapshot
  PRIVATE_DEPS
    pw_cpu_exception_cortex_m.config
    pw_cpu_exception_cortex_m.constants
    pw_cpu_exception_cortex_m.proto_dump
    pw_cpu_exception_cortex_m.util
    pw_log
    pw_polyfill.span
  SOURCES
    snapshot.cc
  HEADERS
    public/pw_cpu_exception_cortex_m/snapshot.h
)

pw_add_module_library(pw_cpu_exception_cortex_m.constants
  PUBLIC_DEPS
    pw_preprocessor.arch
  HEADERS
    pw_cpu_exception_cortex_m_private/cortex_m_constants.h
)

# TODO(pwbug/583): Add ARMv8-M mainline coverage.
if("${pw_cpu_exception.entry_BACKEND}" STREQUAL
   "pw_cpu_exception_cortex_m.cpu_exception")
  pw_add_test(pw_cpu_exception_cortex_m.cpu_exception_entry_test
    SOURCES
      exception_entry_test.cc
    DEPS
      pw_cpu_exception.entry
      pw_cpu_exception.handler
      pw_cpu_exception.support
      pw_cpu_exception_cortex_m.cortex_m_constants
      pw_cpu_exception_cortex_m.cpu_exception
      pw_cpu_exception_cortex_m.cpu_state
      pw_polyfill.span
    GROUPS
      modules
      pw_cpu_exception_cortex_m
  )

  pw_add_test(pw_cpu_exception_cortex_m.util_test
    SOURCES
      util_test.cc
    DEPS
      pw_cpu_exception_cortex_m.cpu_state
      pw_cpu_exception_cortex_m.util
    GROUPS
      modules
      pw_cpu_exception_cortex_m
  )
endif()
