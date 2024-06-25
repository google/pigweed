// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_hex_dump/log_bytes.h"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "pw_log/log.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace pw::dump {
namespace {

std::array<const std::byte, 15> short_string = {
    std::byte('m'),
    std::byte('y'),
    std::byte(' '),
    std::byte('t'),
    std::byte('e'),
    std::byte('s'),
    std::byte('t'),
    std::byte(' '),
    std::byte('s'),
    std::byte('t'),
    std::byte('r'),
    std::byte('i'),
    std::byte('n'),
    std::byte('g'),
    std::byte('\n'),
};

std::array<const std::byte, 33> long_buffer = {
    std::byte(0xa4), std::byte(0xcc), std::byte(0x32), std::byte(0x62),
    std::byte(0x9b), std::byte(0x46), std::byte(0x38), std::byte(0x1a),
    std::byte(0x23), std::byte(0x1a), std::byte(0x2a), std::byte(0x7a),
    std::byte(0xbc), std::byte(0xe2), std::byte(0x40), std::byte(0xa0),
    std::byte(0xff), std::byte(0x33), std::byte(0xe5), std::byte(0x2b),
    std::byte(0x9e), std::byte(0x9f), std::byte(0x6b), std::byte(0x3c),
    std::byte(0xbe), std::byte(0x9b), std::byte(0x89), std::byte(0x3c),
    std::byte(0x7e), std::byte(0x4a), std::byte(0x7a), std::byte(0x48),
    std::byte(0x18)};

TEST(LogBytes, ShortString) {
  LogBytes(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes(PW_LOG_LEVEL_INFO, short_string);
  LogBytes(PW_LOG_LEVEL_WARN, short_string);
  LogBytes(PW_LOG_LEVEL_ERROR, short_string);
  LogBytes(PW_LOG_LEVEL_CRITICAL, short_string);
}

TEST(LogBytes, BytesPerLine) {
  LogBytes<0>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<1>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<2>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<3>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<4>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<8>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<16>(PW_LOG_LEVEL_DEBUG, short_string);
  LogBytes<16>(PW_LOG_LEVEL_DEBUG, long_buffer);
  LogBytes<32>(PW_LOG_LEVEL_DEBUG, long_buffer);
}

TEST(LogBytes, LongBuffer) {
  LogBytes(PW_LOG_LEVEL_DEBUG, long_buffer);
  LogBytes(PW_LOG_LEVEL_INFO, long_buffer);
  LogBytes(PW_LOG_LEVEL_WARN, long_buffer);
  LogBytes(PW_LOG_LEVEL_ERROR, long_buffer);
  LogBytes(PW_LOG_LEVEL_CRITICAL, long_buffer);
}

}  // namespace
}  // namespace pw::dump
