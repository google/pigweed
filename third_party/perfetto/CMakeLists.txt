# Copyright 2024 The Pigweed Authors
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

set(dir_pw_third_party_perfetto "" CACHE PATH "Path to the perfetto installation.")
option(pw_third_party_perfetto_ADD_SUBDIRECTORY
    "Whether to add the dir_pw_third_party_perfetto subdirectory" OFF)

if("${dir_pw_third_party_perfetto}" STREQUAL "")
  pw_add_error_target(pw_third_party.perfetto
    MESSAGE
      "Attempted to use perfetto without configuring it, see "
      "pigweed.dev/third_party/perfetto/"
  )
  return()
elseif(pw_third_party_perfetto_ADD_SUBDIRECTORY)
  add_subdirectory("${dir_pw_third_party_perfetto}" third_party/perfetto)
endif()

pw_proto_library(pw_third_party.perfetto.proto
  SOURCES
    "${dir_pw_third_party_perfetto}/protos/perfetto/trace/perfetto_trace.proto"
  STRIP_PREFIX
    "${dir_pw_third_party_perfetto}/protos/perfetto/trace"
  PREFIX
    "perfetto"
)
