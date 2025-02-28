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
#include "pw_unit_test/framework.h"

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

  EXPECT_FALSE(packet.HasReleaseFn());
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

  EXPECT_FALSE(packet.HasReleaseFn());
}

TEST(H4Packet, H4PacketWithH4WithEmptyBuffer) {
  std::array<uint8_t, 0> h4_buffer{};
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

  EXPECT_TRUE(packet.GetH4Span().empty());

  EXPECT_TRUE(packet.GetHciSpan().empty());

  EXPECT_FALSE(packet.HasReleaseFn());
}

TEST(H4Packet, H4PacketWithWithTypeCtorWithEmptyBuffer) {
  std::array<uint8_t, 0> h4_buffer{};
  H4PacketWithH4 packet{emboss::H4PacketType::COMMAND, pw::span{h4_buffer}};

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

  EXPECT_TRUE(packet.GetH4Span().empty());

  EXPECT_TRUE(packet.GetHciSpan().empty());

  EXPECT_FALSE(packet.HasReleaseFn());
}

TEST(H4Packet, H4PacketWithH4Sets) {
  std::array<uint8_t, 5> h4_buffer = {0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  H4PacketWithH4 packet{pw::span{h4_buffer}};

  packet.SetH4Type(emboss::H4PacketType::EVENT);

  EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
  EXPECT_EQ(h4_buffer[0], cpp23::to_underlying(emboss::H4PacketType::EVENT));
}

TEST(H4PacketRelease, EmptyReleaseFn) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  H4PacketWithH4 packet(pw::span{h4_buffer});
  EXPECT_FALSE(packet.HasReleaseFn());

  H4PacketWithH4 packet2(emboss::H4PacketType::EVENT, pw::span{h4_buffer});
  EXPECT_FALSE(packet2.HasReleaseFn());

  H4PacketWithH4 packet3(pw::span{h4_buffer}, nullptr);
  EXPECT_FALSE(packet3.HasReleaseFn());
}

TEST(H4PacketRelease, ReleaseCalledOnDtor) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  const uint8_t* released_h4_buffer = nullptr;
  {
    H4PacketWithH4 packet{
        pw::span{h4_buffer},
        [&released_h4_buffer](const uint8_t* h4_buffer_to_release) {
          released_h4_buffer = h4_buffer_to_release;
        }};
    EXPECT_TRUE(packet.HasReleaseFn());
  }

  // release_fn was called with h4_buffer* by the time packet went out of scope
  EXPECT_TRUE(released_h4_buffer);
  EXPECT_EQ(released_h4_buffer, h4_buffer.data());
}

TEST(H4PacketRelease, ReleaseCalledAfterMoveOnDtor) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  const uint8_t* released_h4_buffer = nullptr;
  {
    H4PacketWithH4 packet{
        pw::span{h4_buffer},
        [&released_h4_buffer](const uint8_t* h4_buffer_to_release) {
          released_h4_buffer = h4_buffer_to_release;
        }};

    EXPECT_TRUE(packet.HasReleaseFn());

    H4PacketWithH4 packet2(std::move(packet));

    // packet was reset by packet2 move
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_FALSE(packet.HasReleaseFn());
    EXPECT_TRUE(packet.GetHciSpan().empty());
    EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

    // release_fn was not called during move
    EXPECT_EQ(released_h4_buffer, nullptr);
  }

  // release_fn was called with h4_buffer* by the time packet2 went out of scope
  EXPECT_NE(released_h4_buffer, nullptr);
  EXPECT_EQ(released_h4_buffer, h4_buffer.data());
}

TEST(H4PacketRelease, ReleaseCalledAfterMoveAssignOnDtor) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  const uint8_t* released_h4_buffer = nullptr;
  {
    H4PacketWithH4 packet{
        pw::span{h4_buffer},
        [&released_h4_buffer](const uint8_t* h4_buffer_to_release) {
          released_h4_buffer = h4_buffer_to_release;
        }};

    EXPECT_TRUE(packet.HasReleaseFn());

    H4PacketWithH4 packet2{{}};
    packet2 = std::move(packet);

    // packet was reset by packet2 move assign
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_FALSE(packet.HasReleaseFn());
    EXPECT_TRUE(packet.GetHciSpan().empty());
    EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

    // release_fn was not called during move assign
    EXPECT_EQ(released_h4_buffer, nullptr);
  }

  // release_fn was called with h4_buffer* by the time packet2 went out of scope
  EXPECT_NE(released_h4_buffer, nullptr);
  EXPECT_EQ(released_h4_buffer, h4_buffer.data());
}

TEST(H4PacketRelease, ResetAndReturnReleaseFn) {
  std::array<uint8_t, 5> h4_buffer{0, 1, 2, 3, 4};
  h4_buffer[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);

  const uint8_t* released_h4_buffer = nullptr;
  {
    H4PacketWithH4 packet{
        pw::span{h4_buffer},
        [&released_h4_buffer](const uint8_t* h4_buffer_to_release) {
          released_h4_buffer = h4_buffer_to_release;
        }};
    EXPECT_TRUE(packet.HasReleaseFn());

    pw::span<uint8_t> span_from_packet = packet.GetH4Span();
    EXPECT_FALSE(span_from_packet.empty());

    pw::Function<void(const uint8_t*)> release_fn =
        packet.ResetAndReturnReleaseFn();
    EXPECT_TRUE(release_fn);

    // packet was reset by ResetAndReturnReleaseFn
    EXPECT_FALSE(packet.HasReleaseFn());
    EXPECT_TRUE(packet.GetHciSpan().empty());
    EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

    // release_fn passed to packet was not called yet
    EXPECT_EQ(released_h4_buffer, nullptr);

    release_fn(span_from_packet.data());

    // release_fn passed to packet and returned above was called with h4_buffer*
    EXPECT_TRUE(released_h4_buffer);

    // Set to null so we can verify outside of scope that release_fn was
    // not called again.
    released_h4_buffer = nullptr;
  }

  // release_fn passed to packet was not called by the time packet went out of
  // scope
  EXPECT_EQ(released_h4_buffer, nullptr);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
