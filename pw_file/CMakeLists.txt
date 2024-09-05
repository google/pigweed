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

pw_add_library(pw_file.flat_file_system STATIC
  HEADERS
    public/pw_file/flat_file_system.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_file.proto.pwpb
    pw_file.proto.raw_rpc
    pw_log
    pw_result
    pw_span
    pw_status
  SOURCES
    flat_file_system.cc
)

pw_proto_library(pw_file.proto
  SOURCES
    file.proto
  DEPS
     pw_protobuf.common_proto
  PREFIX
    pw_file
)

pw_add_test(pw_file.flat_file_system_test
  SOURCES
    flat_file_system_test.cc
  PRIVATE_DEPS
    pw_bytes
    pw_file.flat_file_system
    pw_file.proto.pwpb
    pw_protobuf
    pw_rpc.raw.test_method_context
    pw_status
  GROUPS
    modules
    pw_file
)
