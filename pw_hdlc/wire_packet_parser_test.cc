// Copyright 2021 The Pigweed Authors
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

#include "pw_hdlc/wire_packet_parser.h"

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_hdlc_private/protocol.h"

namespace pw::hdlc {
namespace {

constexpr uint8_t kAddress = 0x6a;
constexpr uint8_t kControl = 0x03;

TEST(WirePacketParser, Parse_ValidPacket) {
  WirePacketParser parser;
  EXPECT_TRUE(parser.Parse(bytes::Concat(
      kFlag, kAddress, kControl, bytes::String("hello"), 0xc40cefe0, kFlag)));
  auto maybe_address = parser.GetDestinationAddress();
  EXPECT_TRUE(maybe_address.has_value());
  EXPECT_EQ(maybe_address.value(), kAddress);
}

TEST(WirePacketParser, Parse_EscapedAddress) {
  WirePacketParser parser;
  EXPECT_TRUE(parser.Parse(bytes::Concat(kFlag,
                                         kEscape,
                                         uint8_t{0x7e ^ 0x20},
                                         kControl,
                                         bytes::String("hello"),
                                         0x579d573d,
                                         kFlag)));
  auto maybe_address = parser.GetDestinationAddress();
  EXPECT_TRUE(maybe_address.has_value());
  EXPECT_EQ(maybe_address.value(), 0x7eu);
}

TEST(WirePacketParser, Parse_EscapedPayload) {
  WirePacketParser parser;
  EXPECT_TRUE(parser.Parse(bytes::Concat(kFlag,
                                         kAddress,
                                         kControl,
                                         bytes::String("hello"),
                                         kEscapedEscape,
                                         bytes::String("world"),
                                         0x9a9b934a,
                                         kFlag)));
  auto maybe_address = parser.GetDestinationAddress();
  EXPECT_TRUE(maybe_address.has_value());
  EXPECT_EQ(maybe_address.value(), kAddress);
}

TEST(WirePacketParser, Parse_EscapedFcs) {
  WirePacketParser parser;
  EXPECT_TRUE(parser.Parse(bytes::Concat(kFlag,
                                         kAddress,
                                         kControl,
                                         bytes::String("qwertyu"),
                                         // FCS: e0 7d e9 3b
                                         bytes::String("\x3b\xe9\x7d\x5d\xe0"),
                                         kFlag)));
  auto maybe_address = parser.GetDestinationAddress();
  EXPECT_TRUE(maybe_address.has_value());
  EXPECT_EQ(maybe_address.value(), kAddress);
}

TEST(WirePacketParser, Parse_MultipleEscapes) {
  WirePacketParser parser;
  EXPECT_TRUE(parser.Parse(bytes::Concat(kFlag,
                                         kEscapedFlag,
                                         kControl,
                                         kEscapedEscape,
                                         kEscapedFlag,
                                         kEscapedFlag,
                                         0xc85df51d,
                                         kFlag)));
  auto maybe_address = parser.GetDestinationAddress();
  EXPECT_TRUE(maybe_address.has_value());
  EXPECT_EQ(maybe_address.value(), static_cast<uint32_t>(kFlag));
}

TEST(WirePacketParser, Parse_AddressBits) {
  WirePacketParser parser(4);
  EXPECT_TRUE(parser.Parse(bytes::Concat(kFlag,
                                         std::byte{0xab},
                                         kControl,
                                         bytes::String("hello"),
                                         0xae667bdf,
                                         kFlag)));
  auto maybe_address = parser.GetDestinationAddress();
  EXPECT_TRUE(maybe_address.has_value());
  EXPECT_EQ(maybe_address.value(), 0xau);
}

TEST(WirePacketParser, Parse_BadFcs) {
  WirePacketParser parser;
  EXPECT_FALSE(parser.Parse(bytes::Concat(
      kFlag, kAddress, kControl, bytes::String("hello"), 0x1badda7a, kFlag)));
}

TEST(WirePacketParser, Parse_DoubleEscape) {
  WirePacketParser parser;
  EXPECT_FALSE(parser.Parse(bytes::Concat(
      kFlag, kAddress, kControl, bytes::String("hello"), 0x01027d7d, kFlag)));
}

TEST(WirePacketParser, Parse_FlagInFrame) {
  WirePacketParser parser;
  EXPECT_FALSE(parser.Parse(bytes::Concat(
      kFlag, kAddress, kControl, bytes::String("he~lo"), 0xdbae98fe, kFlag)));
}

TEST(WirePacketParser, Parse_EmptyPacket) {
  WirePacketParser parser;
  EXPECT_FALSE(parser.Parse({}));
}

}  // namespace
}  // namespace pw::hdlc
