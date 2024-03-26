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
include($ENV{PW_ROOT}/pw_async2/backend.cmake)

pw_add_library(pw_channel INTERFACE
  HEADERS
    public/pw_channel/channel.h
  PUBLIC_DEPS
    pw_assert
    pw_async2.dispatcher
    pw_async2.poll
    pw_bytes
    pw_multibuf
    pw_multibuf.allocator
    pw_result
    pw_span
    pw_status
    pw_toolchain._sibling_cast
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_channel.forwarding_channel STATIC
  HEADERS
    public/pw_channel/forwarding_channel.h
  SOURCES
    forwarding_channel.cc
  PUBLIC_DEPS
    pw_channel
    pw_multibuf.allocator
    pw_sync.mutex
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_channel.forwarding_channel_test
  SOURCES
    forwarding_channel_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_channel.forwarding_channel
    pw_multibuf.header_chunk_region_tracker
    pw_multibuf.simple_allocator
)

pw_add_library(pw_channel.loopback_channel STATIC
  HEADERS
    public/pw_channel/loopback_channel.h
  SOURCES
    loopback_channel.cc
  PUBLIC_DEPS
    pw_channel
    pw_multibuf.allocator
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_channel.loopback_channel_test
  SOURCES
    loopback_channel_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_channel.loopback_channel
    pw_multibuf.simple_allocator
)
