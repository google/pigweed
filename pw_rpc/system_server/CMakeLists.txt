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

include("$ENV{PW_ROOT}/pw_build/pigweed.cmake")

pw_add_facade(pw_rpc.system_server
  PUBLIC_DEPS
    pw_rpc.server
    pw_stream
)
target_include_directories(pw_rpc.system_server.facade INTERFACE public)

pw_add_module_library(pw_rpc.system_server.socket
  IMPLEMENTS_FACADE
    pw_rpc.system_server
  SOURCES
    socket_rpc_server.cc
  PRIVATE_DEPS
    pw_hdlc_lite
    pw_rpc.server
    pw_stream.socket_stream
)

pw_add_module_library(pw_rpc.system_server.sys_io
  IMPLEMENTS_FACADE
    pw_rpc.system_server
  SOURCES
    sys_io_rpc_server.cc
  PUBLIC_DEPS
    pw_hdlc_lite
    pw_stream.sys_io_stream
)
