// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/l2cap/pdu.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fragmenter.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/recombiner.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/transport/packet.h"

namespace bt::l2cap {
namespace {

template <typename... T>
hci::ACLDataPacketPtr PacketFromBytes(T... data) {
  StaticByteBuffer bytes(std::forward<T>(data)...);
  BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

  auto packet =
      hci::ACLDataPacket::New(bytes.size() - sizeof(hci_spec::ACLDataHeader));
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();

  return packet;
}

TEST(PduTest, CanCopyEmptyBody) {
  Recombiner recombiner(0x0001);

  // clang-format off

  auto packet = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x04, 0x00,

    // Basic l2cap header
    0x00, 0x00, 0xFF, 0xFF
  );

  // clang-format on

  auto result = recombiner.ConsumeFragment(std::move(packet));
  ASSERT_TRUE(result.pdu);

  PDU pdu = std::move(*result.pdu);
  ASSERT_TRUE(pdu.is_valid());
  ASSERT_EQ(1u, pdu.fragment_count());
  ASSERT_EQ(0u, pdu.length());

  DynamicByteBuffer buf(0);
  EXPECT_EQ(0u, pdu.Copy(&buf));
}

TEST(PduTest, Move) {
  Recombiner recombiner(0x0001);

  // clang-format off

  auto packet = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x08, 0x00,

    // Basic l2cap header
    0x04, 0x00, 0xFF, 0xFF, 'T', 'e', 's', 't'
  );

  // clang-format on

  auto result = recombiner.ConsumeFragment(std::move(packet));
  ASSERT_TRUE(result.pdu);

  PDU pdu = std::move(*result.pdu);
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(1u, pdu.fragment_count());

  StaticByteBuffer<4> pdu_data;

  // Read the entire PDU.
  EXPECT_EQ(4u, pdu.Copy(&pdu_data));
  EXPECT_EQ("Test", pdu_data.AsString());

  PDU move_cted(std::move(pdu));
  EXPECT_FALSE(pdu.is_valid());
  EXPECT_EQ(0u, pdu.fragment_count());
  EXPECT_TRUE(move_cted.is_valid());
  EXPECT_EQ(1u, move_cted.fragment_count());

  pdu_data.SetToZeros();
  EXPECT_EQ(4u, move_cted.Copy(&pdu_data));
  EXPECT_EQ("Test", pdu_data.AsString());

  PDU move_assigned = std::move(move_cted);
  EXPECT_FALSE(move_cted.is_valid());
  EXPECT_EQ(0u, move_cted.fragment_count());
  EXPECT_TRUE(move_assigned.is_valid());
  EXPECT_EQ(1u, move_assigned.fragment_count());

  pdu_data.SetToZeros();
  EXPECT_EQ(4u, move_assigned.Copy(&pdu_data));
  EXPECT_EQ("Test", pdu_data.AsString());
}

TEST(PduTest, ReleaseFragments) {
  Recombiner recombiner(0x0001);

  // clang-format off

  auto packet = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x08, 0x00,

    // Basic l2cap header
    0x04, 0x00, 0xFF, 0xFF, 'T', 'e', 's', 't'
  );

  // clang-format on

  auto result = recombiner.ConsumeFragment(std::move(packet));
  ASSERT_TRUE(result.pdu);

  PDU pdu = std::move(*result.pdu);
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(1u, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();

  EXPECT_FALSE(pdu.is_valid());
  ASSERT_FALSE(fragments.empty());
  EXPECT_EQ(0u, pdu.fragment_count());

  // Directly count the elements in |fragments| to make sure the count is
  // correct.
  size_t count = 0;
  for ([[maybe_unused]] const auto& f : fragments)
    count++;
  EXPECT_EQ(1u, count);

  // Check that the fragment we got out is identical to the one we fed in.
  EXPECT_TRUE(ContainersEqual(StaticByteBuffer(
                                  // ACL data header
                                  0x01,
                                  0x00,
                                  0x08,
                                  0x00,

                                  // Basic l2cap header
                                  0x04,
                                  0x00,
                                  0xFF,
                                  0xFF,
                                  'T',
                                  'e',
                                  's',
                                  't'),
                              (*fragments.begin())->view().data()));
}

TEST(PduTest, ReadSingleFragment) {
  Recombiner recombiner(0x0001);

  // clang-format off

  auto packet = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x08, 0x00,

    // Basic l2cap header
    0x04, 0x00, 0xFF, 0xFF, 'T', 'e', 's', 't'
  );

  // clang-format on

  auto result = recombiner.ConsumeFragment(std::move(packet));
  ASSERT_TRUE(result.pdu);

  PDU pdu = std::move(*result.pdu);
  EXPECT_TRUE(pdu.is_valid());

  StaticByteBuffer<4> pdu_data;

  // Read the entire PDU.
  EXPECT_EQ(4u, pdu.Copy(&pdu_data));
  EXPECT_EQ("Test", pdu_data.AsString());

  // Read 1 byte at offset 1.
  pdu_data.Fill('X');
  EXPECT_EQ(1u, pdu.Copy(&pdu_data, 1, 1));
  EXPECT_EQ("eXXX", pdu_data.AsString());

  // Read bytes starting at offset 2.
  pdu_data.Fill('X');
  EXPECT_EQ(2u, pdu.Copy(&pdu_data, 2));
  EXPECT_EQ("stXX", pdu_data.AsString());

  // Read bytes starting at offset 3.
  pdu_data.Fill('X');
  EXPECT_EQ(1u, pdu.Copy(&pdu_data, 3));
  EXPECT_EQ("tXXX", pdu_data.AsString());
}

TEST(PduTest, ReadMultipleFragments) {
  Recombiner recombiner(0x0001);

  // clang-format off

  // Partial initial fragment
  auto packet0 = PacketFromBytes(
    // ACL data header (PBF: initial fragment)
    0x01, 0x00, 0x0A, 0x00,

    // Basic l2cap header
    0x0F, 0x00, 0xFF, 0xFF, 'T', 'h', 'i', 's', ' ', 'i'
  );

  // Continuation fragment
  auto packet1 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x06, 0x00,

    // L2CAP PDU fragment
    's', ' ', 'a', ' ', 't', 'e'
  );

  // Continuation fragment
  auto packet2 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x02, 0x00,

    // L2CAP PDU fragment
    's', 't'
  );

  // Continuation fragment
  auto packet3 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x01, 0x00,

    // L2CAP PDU fragment
    '!'
  );

  EXPECT_FALSE(recombiner.ConsumeFragment(std::move(packet0)).frames_dropped);
  EXPECT_FALSE(recombiner.ConsumeFragment(std::move(packet1)).frames_dropped);
  EXPECT_FALSE(recombiner.ConsumeFragment(std::move(packet2)).frames_dropped);
  auto result = recombiner.ConsumeFragment(std::move(packet3));
  EXPECT_FALSE(result.frames_dropped);
  ASSERT_TRUE(result.pdu);

  PDU pdu = std::move(*result.pdu);
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(4u, pdu.fragment_count());

  StaticByteBuffer<15> pdu_data;

  // Read the entire PDU.
  EXPECT_EQ(15u, pdu.Copy(&pdu_data));
  EXPECT_EQ("This is a test!", pdu_data.AsString());

  // Read 1 byte at offset 1.
  pdu_data.Fill('X');
  EXPECT_EQ(1u, pdu.Copy(&pdu_data, 1, 1));
  EXPECT_EQ("hXXXXXXXXXXXXXX", pdu_data.AsString());

  // Read bytes starting at offset 2.
  pdu_data.Fill('X');
  EXPECT_EQ(13u, pdu.Copy(&pdu_data, 2));
  EXPECT_EQ("is is a test!XX", pdu_data.AsString());

  // Read bytes starting at the last octet of the first fragment.
  pdu_data.Fill('X');
  EXPECT_EQ(10u, pdu.Copy(&pdu_data, 5));
  EXPECT_EQ("is a test!XXXXX", pdu_data.AsString());

  // Read bytes starting at the first octet of the second fragment.
  pdu_data.Fill('X');
  EXPECT_EQ(9u, pdu.Copy(&pdu_data, 6));
  EXPECT_EQ("s a test!XXXXXX", pdu_data.AsString());

  // Read the very last octet from the last fragment.
  pdu_data.Fill('X');
  EXPECT_EQ(1u, pdu.Copy(&pdu_data, 14));
  EXPECT_EQ("!XXXXXXXXXXXXXX", pdu_data.AsString());

  // Partial read across multiple fragments
  pdu_data.Fill('X');
  EXPECT_EQ(8u, pdu.Copy(&pdu_data, 5, 8));
  EXPECT_EQ("is a tesXXXXXXX", pdu_data.AsString());
}

}  // namespace
}  // namespace bt
