// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "recombiner.h"
#include "pdu.h"

#include "gtest/gtest.h"

#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/packet.h"

namespace bt {
namespace l2cap {
namespace {

template <typename... T>
hci::ACLDataPacketPtr PacketFromBytes(T... data) {
  auto bytes = common::CreateStaticByteBuffer(std::forward<T>(data)...);
  ZX_DEBUG_ASSERT(bytes.size() >= sizeof(hci::ACLDataHeader));

  auto packet =
      hci::ACLDataPacket::New(bytes.size() - sizeof(hci::ACLDataHeader));
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();

  return packet;
}

TEST(L2CAP_RecombinerTest, BadFirstFragment) {
  Recombiner recombiner;
  PDU pdu;
  EXPECT_FALSE(pdu.is_valid());

  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));

  // clang-format off

  // Too small (no basic L2CAP header)
  auto packet0 = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x03, 0x00,

    // Incomplete basic L2CAP header (one byte short)
    0x00, 0x00, 0x03
  );

  // Too large
  auto packet1 = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x06, 0x00,

    // PDU length (2 bytes) is larger than reported length (1 byte)
    0x01, 0x00, 0xFF, 0xFF, 0x00, 0x00
  );

  // Continuation fragment
  auto packet2 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x05, 0x00,

    // L2CAP header + PDU
    0x01, 0x00, 0xFF, 0xFF, 0x00
  );

  // clang-format on

  EXPECT_FALSE(recombiner.AddFragment(std::move(packet0)));
  EXPECT_FALSE(recombiner.AddFragment(std::move(packet1)));
  EXPECT_FALSE(recombiner.AddFragment(std::move(packet2)));

  // None of the packets should have been moved.
  EXPECT_TRUE(packet0);
  EXPECT_TRUE(packet1);
  EXPECT_TRUE(packet2);

  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));
}

TEST(L2CAP_RecombinerTest, CompleteFirstFragmentEmptyPDU) {
  Recombiner recombiner;

  // clang-format off

  // Zero-length PDU
  auto packet0 = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x04, 0x00,

    // Basic L2CAP header
    0x00, 0x00, 0xFF, 0xFF
  );

  // Continuation fragment
  auto packet1 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x05, 0x00,

    // L2CAP header + PDU
    0x01, 0x00, 0xFF, 0xFF, 0x00
  );

  // clang-format on

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));

  // |packet0| should have moved.
  EXPECT_FALSE(packet0);

  EXPECT_FALSE(recombiner.empty());
  EXPECT_TRUE(recombiner.ready());

  // Adding a continuation fragment should fail as the PDU is complete.
  EXPECT_FALSE(recombiner.AddFragment(std::move(packet1)));
  EXPECT_TRUE(packet1);

  PDU pdu;
  EXPECT_TRUE(recombiner.Release(&pdu));
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(0u, pdu.length());
  EXPECT_EQ(0xFFFF, pdu.channel_id());

  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));

  // PDU should remain unmodified.
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(0u, pdu.length());
  EXPECT_EQ(0xFFFF, pdu.channel_id());
}

TEST(L2CAP_RecombinerTest, CompleteFirstFragment) {
  Recombiner recombiner;

  // clang-format off

  // Non-empty PDU
  auto packet0 = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x08, 0x00,

    // Basic L2CAP header
    0x04, 0x00, 0xFF, 0xFF, 'T', 'e', 's', 't'
  );

  // Continuation fragment
  auto packet1 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x05, 0x00,

    // L2CAP header + PDU
    0x01, 0x00, 0xFF, 0xFF, 0x00
  );

  // clang-format on

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));

  // |packet0| should have moved.
  EXPECT_FALSE(packet0);

  EXPECT_FALSE(recombiner.empty());
  EXPECT_TRUE(recombiner.ready());

  // Adding a continuation fragment should fail as the PDU is complete.
  EXPECT_FALSE(recombiner.AddFragment(std::move(packet1)));
  EXPECT_TRUE(packet1);

  PDU pdu;
  EXPECT_TRUE(recombiner.Release(&pdu));
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(0xFFFF, pdu.channel_id());

  ASSERT_EQ(4u, pdu.length());

  common::StaticByteBuffer<4> pdu_data;
  pdu.Copy(&pdu_data);
  EXPECT_EQ("Test", pdu_data.AsString());

  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));
}

TEST(L2CAP_RecombinerTest, BadContinuationFragment) {
  Recombiner recombiner;

  // clang-format off

  // Partial initial fragment
  auto packet0 = PacketFromBytes(
    // ACL data header (total size is 4, packet contains 1)
    0x01, 0x00, 0x05, 0x00,

    // Basic L2CAP header
    0x04, 0x00, 0xFF, 0xFF, 'T'
  );

  // Beginning fragment (instead of continuation).
  auto packet1 = PacketFromBytes(
    // ACL data header (PBF: first non-flushable)
    0x01, 0x00, 0x03, 0x00,

    // L2CAP PDU fragment
    'e', 's', 't'
  );

  // Continuation fragment too long
  auto packet2 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x04, 0x00,

    // L2CAP PDU fragment
    'e', 's', 't', '!'
  );

  // clang-format on

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));
  EXPECT_FALSE(packet0);

  // recombiner is no longer empty but also not ready yet.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_FALSE(recombiner.ready());

  EXPECT_FALSE(recombiner.AddFragment(std::move(packet1)));
  EXPECT_FALSE(recombiner.AddFragment(std::move(packet2)));
  EXPECT_TRUE(packet1);
  EXPECT_TRUE(packet2);

  // recombiner state should not have changed.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_FALSE(recombiner.ready());
}

TEST(L2CAP_RecombinerTest, CompleteContinuationFragment) {
  Recombiner recombiner;

  // clang-format off

  // Partial initial fragment
  auto packet0 = PacketFromBytes(
    // ACL data header (total size is 4, packet contains 1)
    0x01, 0x00, 0x05, 0x00,

    // Basic L2CAP header
    0x04, 0x00, 0xFF, 0xFF, 'T'
  );

  // Continuation fragment
  auto packet1 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x03, 0x00,

    // L2CAP PDU fragment
    'e', 's', 't'
  );

  // clang-format on

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));
  EXPECT_FALSE(packet0);

  // recombiner is no longer empty but also not ready yet.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_FALSE(recombiner.ready());

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet1)));
  EXPECT_FALSE(packet1);

  // recombiner should be ready.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_TRUE(recombiner.ready());

  PDU pdu;
  EXPECT_TRUE(recombiner.Release(&pdu));
  EXPECT_TRUE(pdu.is_valid());
  EXPECT_EQ(0xFFFF, pdu.channel_id());

  ASSERT_EQ(4u, pdu.length());

  common::StaticByteBuffer<4> pdu_data;
  pdu.Copy(&pdu_data);
  EXPECT_EQ("Test", pdu_data.AsString());

  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));
}

TEST(L2CAP_RecombinerTest, MultipleContinuationFragments) {
  Recombiner recombiner;

  // clang-format off

  // Partial initial fragment
  auto packet0 = PacketFromBytes(
    // ACL data header (total size is 4, packet contains 1)
    0x01, 0x00, 0x0A, 0x00,

    // Basic L2CAP header
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

  // clang-format on

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));
  EXPECT_FALSE(packet0);

  // recombiner is no longer empty but also not ready yet.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_FALSE(recombiner.ready());

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet1)));
  EXPECT_TRUE(recombiner.AddFragment(std::move(packet2)));
  EXPECT_TRUE(recombiner.AddFragment(std::move(packet3)));
  EXPECT_FALSE(packet1);
  EXPECT_FALSE(packet2);
  EXPECT_FALSE(packet3);

  // recombiner should be ready.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_TRUE(recombiner.ready());

  PDU pdu;
  EXPECT_TRUE(recombiner.Release(&pdu));
  EXPECT_TRUE(pdu.is_valid());

  ASSERT_EQ(15u, pdu.length());

  common::StaticByteBuffer<15u> pdu_data;
  pdu.Copy(&pdu_data);
  EXPECT_EQ("This is a test!", pdu_data.AsString());

  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));
}

TEST(L2CAP_RecombinerTest, Drop) {
  Recombiner recombiner;

  // clang-format off

  // Partial initial fragment
  auto packet0 = PacketFromBytes(
    // ACL data header (total size is 4, packet contains 1)
    0x01, 0x00, 0x05, 0x00,

    // Basic L2CAP header
    0x04, 0x00, 0xFF, 0xFF, 'T'
  );

  // Continuation fragment
  auto packet1 = PacketFromBytes(
    // ACL data header (PBF: continuing fragment)
    0x01, 0x10, 0x03, 0x00,

    // L2CAP PDU fragment
    'e', 's', 't'
  );

  // clang-format on

  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));
  EXPECT_TRUE(recombiner.AddFragment(std::move(packet1)));

  // recombiner should be ready.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_TRUE(recombiner.ready());

  recombiner.Drop();

  PDU pdu;
  EXPECT_FALSE(recombiner.ready());
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.Release(&pdu));
}

TEST(L2CAP_RecombinerTest, DropPartial) {
  Recombiner recombiner;

  // clang-format off

  // Partial initial fragment
  auto packet0 = PacketFromBytes(
    // ACL data header (total size is 4, packet contains 1)
    0x01, 0x00, 0x05, 0x00,

    // Basic L2CAP header
    0x04, 0x00, 0xFF, 0xFF, 'T'
  );

  // Complete packet
  auto packet1 = PacketFromBytes(
    // ACL data header
    0x01, 0x00, 0x08, 0x00,

    // Basic L2CAP header
    0x04, 0x00, 0xFF, 0xFF, 'T', 'e', 's', 't'
  );

  // clang-format on

  // Partially fill packet.
  EXPECT_TRUE(recombiner.AddFragment(std::move(packet0)));
  EXPECT_FALSE(recombiner.empty());
  EXPECT_FALSE(recombiner.ready());

  recombiner.Drop();
  EXPECT_TRUE(recombiner.empty());
  EXPECT_FALSE(recombiner.ready());

  // Build a new PDU. The previous packet should have no effect.
  EXPECT_TRUE(recombiner.AddFragment(std::move(packet1)));

  // recombiner should be ready.
  EXPECT_FALSE(recombiner.empty());
  EXPECT_TRUE(recombiner.ready());

  PDU pdu;
  EXPECT_TRUE(recombiner.Release(&pdu));
  EXPECT_EQ(4u, pdu.length());
}

}  // namespace
}  // namespace l2cap
}  // namespace bt
