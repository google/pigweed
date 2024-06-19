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

#include "pw_bluetooth_proxy/h4_packet.h"

#include <cstdint>

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

TEST(H4Packet, H4PacketWithHciGets) {
  std::array<uint8_t, 5> hci_buffer{0, 1, 2, 3, 4};
  H4PacketWithHci packet{emboss::H4PacketType::COMMAND, pw::span{hci_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::COMMAND);

  EXPECT_EQ(packet.GetHciSpan().size(), hci_buffer.size());
  EXPECT_EQ(packet.GetHciSpan().data(), hci_buffer.data());
}

TEST(H4Packet, H4PacketWithHciSets) {
  std::array<uint8_t, 5> hci_buffer{0, 1, 2, 3, 4};
  H4PacketWithHci packet{emboss::H4PacketType::COMMAND, pw::span{hci_buffer}};

  packet.GetH4Type(emboss::H4PacketType::EVENT);

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
}

TEST(H4Packet, H4PacketWithH4Gets) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::COMMAND);

  // HCI span should be h4 buffer without the first byte.
  EXPECT_EQ(packet.GetHciSpan().size(), h4_buffer.size() - 1);
  EXPECT_EQ(packet.GetHciSpan().data(), h4_buffer.data() + 1);
}

TEST(H4Packet, H4PacketWithH4WithEmptyH4Buffer) {
  std::array<uint8_t, 0> h4_buffer{};
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

  EXPECT_TRUE(packet.GetHciSpan().empty());
}

TEST(H4Packet, H4PacketWithH4Sets) {
  std::array<uint8_t, 5> h4_buffer = {0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  packet.GetH4Type(emboss::H4PacketType::EVENT);

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
  EXPECT_EQ(h4_buffer[0], cpp23::to_underlying(emboss::H4PacketType::EVENT));
}

}  // namespace
}  // namespace pw::bluetooth::proxy
