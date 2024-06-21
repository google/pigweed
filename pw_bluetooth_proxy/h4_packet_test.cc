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

  packet.SetH4Type(emboss::H4PacketType::EVENT);

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
}

TEST(H4Packet, H4PacketWithH4Gets) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::COMMAND);

  EXPECT_EQ(packet.GetH4Span().size(), h4_buffer.size());
  EXPECT_EQ(packet.GetH4Span().data(), h4_buffer.data());

  // HCI span should be h4 buffer without the first byte.
  EXPECT_EQ(packet.GetHciSpan().size(), h4_buffer.size() - 1);
  EXPECT_EQ(packet.GetHciSpan().data(), h4_buffer.data() + 1);
}

TEST(H4Packet, H4PacketWithTypeCtorWithH4Gets) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  H4PacketWithH4 packet{emboss::H4PacketType::COMMAND, pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::COMMAND);

  EXPECT_EQ(packet.GetH4Span().size(), h4_buffer.size());
  EXPECT_EQ(packet.GetH4Span().data(), h4_buffer.data());

  // HCI span should be h4 buffer without the first byte.
  EXPECT_EQ(packet.GetHciSpan().size(), h4_buffer.size() - 1);
  EXPECT_EQ(packet.GetHciSpan().data(), h4_buffer.data() + 1);
}

TEST(H4Packet, H4PacketWithH4WithEmptyBuffer) {
  std::array<uint8_t, 0> h4_buffer{};
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

  EXPECT_TRUE(packet.GetH4Span().empty());

  EXPECT_TRUE(packet.GetHciSpan().empty());
}

TEST(H4Packet, H4PacketWithWithTypeCtorWithEmptyBuffer) {
  std::array<uint8_t, 0> h4_buffer{};
  H4PacketWithH4 packet{emboss::H4PacketType::COMMAND, pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

  EXPECT_TRUE(packet.GetH4Span().empty());

  EXPECT_TRUE(packet.GetHciSpan().empty());
}

TEST(H4Packet, H4PacketWithH4Sets) {
  std::array<uint8_t, 5> h4_buffer = {0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  packet.SetH4Type(emboss::H4PacketType::EVENT);

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
  EXPECT_EQ(h4_buffer[0], cpp23::to_underlying(emboss::H4PacketType::EVENT));
}

TEST(H4PacketRelease, ReleaseCalledOnDtor) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  H4PacketWithH4* released_packet_ptr = nullptr;
  H4PacketWithH4* packet_ptr = nullptr;
  {
    H4PacketWithH4 packet{pw::span{h4_buffer},
                          [&released_packet_ptr](H4PacketWithH4& rel_packet) {
                            released_packet_ptr = &rel_packet;
                          }};
    packet_ptr = &packet;
  }

  // release_fn was called with packet by the time packet went out of scope
  EXPECT_TRUE(released_packet_ptr);
  EXPECT_EQ(released_packet_ptr, packet_ptr);
}

TEST(H4PacketRelease, ReleaseCalledAfterMoveOnDtor) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  H4PacketWithH4* released_packet_ptr = nullptr;
  H4PacketWithH4* packet2_ptr = nullptr;
  {
    H4PacketWithH4 packet{pw::span{h4_buffer},
                          [&released_packet_ptr](H4PacketWithH4& rel_packet) {
                            released_packet_ptr = &rel_packet;
                          }};

    H4PacketWithH4 packet2(std::move(packet));
    packet2_ptr = &packet2;

    // packet was reset by packet2 move ctor
    EXPECT_TRUE(packet.GetHciSpan().empty());
    EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);
    // release_fn was not called during move ctor
    EXPECT_FALSE(released_packet_ptr);
  }
  // release_fn was called with packet2 by the time packet2 went out of scope
  EXPECT_TRUE(released_packet_ptr);
  EXPECT_EQ(released_packet_ptr, packet2_ptr);
}

TEST(H4PacketRelease, ReleaseCalledAfterMoveAssignOnDtor) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  H4PacketWithH4* released_packet_ptr = nullptr;
  H4PacketWithH4* packet2_ptr = nullptr;
  {
    H4PacketWithH4 packet{pw::span{h4_buffer},
                          [&released_packet_ptr](H4PacketWithH4& rel_packet) {
                            released_packet_ptr = &rel_packet;
                          }};

    H4PacketWithH4 packet2{{}};
    packet2_ptr = &packet2;
    packet2 = std::move(packet);

    // packet was reset by packet2 move assign
    EXPECT_TRUE(packet.GetHciSpan().empty());
    EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);
    // release_fn was not called during move ctor
    EXPECT_FALSE(released_packet_ptr);
  }
  // release_fn was called with packet2 by the time packet2 went out of scope
  EXPECT_TRUE(released_packet_ptr);
  EXPECT_EQ(released_packet_ptr, packet2_ptr);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
