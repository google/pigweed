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

pw_add_module_library(pw_rpc.nanopb.method
  SOURCES
    nanopb_method.cc
  PUBLIC_DEPS
    pw_rpc.nanopb.common
    pw_rpc.server
  PRIVATE_DEPS
    pw_log
)

pw_add_module_library(pw_rpc.nanopb.method_union
  PUBLIC_DEPS
    pw_rpc.nanopb.method
    pw_rpc.raw
    pw_rpc.server
  PRIVATE_DEPS
    pw_log
)

pw_add_module_library(pw_rpc.nanopb.client
  SOURCES
    nanopb_client_call.cc
  PUBLIC_DEPS
    pw_rpc.nanopb.common
    pw_rpc.common
)

pw_add_module_library(pw_rpc.nanopb.common
  SOURCES
    nanopb_common.cc
  PUBLIC_DEPS
    pw_bytes
    pw_rpc.common
    pw_third_party.nanopb
)

pw_add_module_library(pw_rpc.nanopb.echo_service
  PUBLIC_DEPS
    pw_rpc.echo_proto.nanopb_rpc
)

pw_auto_add_module_tests(pw_rpc.nanopb
  PRIVATE_DEPS
    pw_rpc.client
    pw_rpc.raw
    pw_rpc.server
    pw_rpc.nanopb.common
    pw_rpc.echo_proto.nanopb_rpc
    pw_rpc.test_protos.nanopb_rpc
    pw_rpc.test_utils
)
