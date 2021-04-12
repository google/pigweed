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

pw_add_module_library(pw_protobuf
  SOURCES
    decoder.cc
    encoder.cc
    find.cc
  PUBLIC_DEPS
    pw_bytes
    pw_result
    pw_status
    pw_varint
)

pw_add_test(pw_protobuf.decoder_test
  SOURCES
    decoder_test.cc
  DEPS
    pw_protobuf
  GROUPS
    modules
    pw_protobuf
)

pw_add_test(pw_protobuf.encoder_test
  SOURCES
    encoder_test.cc
  DEPS
    pw_protobuf
  GROUPS
    modules
    pw_protobuf
)

pw_add_test(pw_protobuf.find_test
  SOURCES
    find_test.cc
  DEPS
    pw_protobuf
  GROUPS
    modules
    pw_protobuf
)

pw_add_test(pw_protobuf.codegen_test
  SOURCES
    codegen_test.cc
  DEPS
    pw_protobuf
    pw_protobuf.codegen_test_protos.pwpb
  GROUPS
    modules
    pw_protobuf
)

pw_proto_library(pw_protobuf.common_protos
  SOURCES
    pw_protobuf_protos/common.proto
)

pw_proto_library(pw_protobuf.codegen_test_protos
  SOURCES
    pw_protobuf_test_protos/full_test.proto
    pw_protobuf_test_protos/imported.proto
    pw_protobuf_test_protos/importer.proto
    pw_protobuf_test_protos/non_pw_package.proto
    pw_protobuf_test_protos/proto2.proto
    pw_protobuf_test_protos/repeated.proto
  DEPS
    pw_protobuf.common_protos
)
