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

pw_auto_add_simple_module(pw_hdlc
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_checksum
    pw_result
    pw_router.packet_parser
    pw_rpc.common
    pw_status
    pw_stream
    pw_sys_io
  PRIVATE_DEPS
    pw_log
)

add_subdirectory(rpc_example)

if(Zephyr_FOUND AND CONFIG_PIGWEED_HDLC)
  zephyr_link_libraries(pw_hdlc)
endif()
