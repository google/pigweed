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

pw_add_library(pw_i2c.address STATIC
  HEADERS
    public/pw_i2c/address.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
  SOURCES
    address.cc
)

pw_add_library(pw_i2c.initiator INTERFACE
  HEADERS
    public/pw_i2c/initiator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_chrono.system_clock
    pw_i2c.address
    pw_status
)

pw_add_library(pw_i2c.device INTERFACE
  HEADERS
    public/pw_i2c/device.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_chrono.system_clock
    pw_i2c.address
    pw_i2c.initiator
    pw_span
    pw_status
)

pw_add_library(pw_i2c.register_device STATIC
  HEADERS
    public/pw_i2c/register_device.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_chrono.system_clock
    pw_i2c.address
    pw_i2c.device
    pw_i2c.initiator
    pw_result
    pw_status
  PRIVATE_DEPS
    pw_assert
  SOURCES
    register_device.cc
)

pw_proto_library(pw_i2c.protos
  SOURCES
    i2c.proto
  INPUTS
    i2c.options
  PREFIX
    pw_i2c
)

pw_add_library(pw_i2c.i2c_service STATIC
  HEADERS
    public/pw_i2c/i2c_service.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_i2c.initiator
    pw_i2c.protos.pwpb_rpc
  PRIVATE_DEPS
    pw_containers.vector
    pw_i2c.address
    pw_status
  SOURCES
    i2c_service.cc
)

pw_add_library(pw_i2c.mock STATIC
  HEADERS
    public/pw_i2c/initiator_mock.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_containers
    pw_containers.to_array
    pw_i2c.initiator
  PRIVATE_DEPS
    pw_assert
    pw_unit_test
  SOURCES
    initiator_mock.cc
)

pw_add_library(pw_i2c.gmock INTERFACE
  HEADERS
    public/pw_i2c/initiator_gmock.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_i2c.initiator
    pw_third_party.googletest
)

pw_add_test(pw_i2c.address_test
  SOURCES
    address_test.cc
  PRIVATE_DEPS
    pw_i2c.address
  GROUPS
    modules
    pw_i2c
)

if(NOT "${pw_chrono.system_clock_BACKEND}" STREQUAL "")
pw_add_test(pw_i2c.device_test
  SOURCES
    device_test.cc
  PRIVATE_DEPS
    pw_containers
    pw_i2c.device
    pw_i2c.mock
  GROUPS
    modules
    pw_i2c
)

pw_add_test(pw_i2c.register_device_test
  SOURCES
    register_device_test.cc
  PRIVATE_DEPS
    pw_assert
    pw_i2c.register_device
  GROUPS
    modules
    pw_i2c
)

pw_add_test(pw_i2c.initiator_mock_test
  SOURCES
    initiator_mock_test.cc
  PUBLIC_DEPS
    pw_containers
    pw_i2c.mock
  GROUPS
    modules
    pw_i2c
)

pw_add_test(pw_i2c.i2c_service_test
  SOURCES
    i2c_service_test.cc
  PUBLIC_DEPS
    pw_containers.vector
    pw_i2c.i2c_service
    pw_i2c.mock
    pw_rpc.test_utils
    pw_status
  GROUPS
    modules
    pw_i2c
)
endif()
