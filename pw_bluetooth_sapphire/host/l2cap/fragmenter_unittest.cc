// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fragmenter.h"

#include "gtest/gtest.h"
#include "pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt {
namespace l2cap {
namespace {

constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr ChannelId kTestChannelId = 0x0001;

TEST(L2CAP_FragmenterTest, EmptyPayload) {
  BufferView payload;

  auto expected_fragment = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x00, 0x04, 0x00,

      // Basic L2CAP header (0-length Information Payload)
      0x00, 0x00, 0x01, 0x00);

  // Make the fragment limit a lot larger than the test frame size.
  Fragmenter fragmenter(kTestHandle, 1024);
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_EQ(1u, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();

  EXPECT_TRUE(
      ContainersEqual(expected_fragment, fragments.begin()->view().data()));
}

TEST(L2CAP_FragmenterTest, SingleFragment) {
  auto payload = CreateStaticByteBuffer('T', 'e', 's', 't');

  auto expected_fragment = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x00, 0x08, 0x00,

      // Basic L2CAP header
      0x04, 0x00, 0x01, 0x00, 'T', 'e', 's', 't');

  // Make the fragment limit a lot larger than the test frame size.
  Fragmenter fragmenter(kTestHandle, 1024);
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_EQ(1u, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();

  EXPECT_TRUE(
      ContainersEqual(expected_fragment, fragments.begin()->view().data()));
}

TEST(L2CAP_FragmenterTest, SingleFragmentExactFit) {
  auto payload = CreateStaticByteBuffer('T', 'e', 's', 't');

  auto expected_fragment = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x00, 0x08, 0x00,

      // Basic L2CAP header
      0x04, 0x00, 0x01, 0x00, 'T', 'e', 's', 't');

  // Make the fragment limit large enough to fit exactly one B-frame containing
  // |payload|.
  Fragmenter fragmenter(kTestHandle, payload.size() + sizeof(BasicHeader));
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_EQ(1u, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();

  EXPECT_TRUE(
      ContainersEqual(expected_fragment, fragments.begin()->view().data()));
}

TEST(L2CAP_FragmenterTest, TwoFragmentsOffByOne) {
  auto payload = CreateStaticByteBuffer('T', 'e', 's', 't', '!');

  auto expected_fragment0 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x00, 0x08, 0x00,

      // Basic L2CAP header, contains the complete length but a partial payload
      0x05, 0x00, 0x01, 0x00, 'T', 'e', 's', 't');

  auto expected_fragment1 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x10, 0x01, 0x00,

      // Continuing payload
      '!');

  // Make the fragment limit large enough to fit exactly one B-frame containing
  // 1 octet less than |payload|. The last octet should be placed in a second
  // fragment.
  Fragmenter fragmenter(kTestHandle, payload.size() + sizeof(BasicHeader) - 1);
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_EQ(2u, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();

  EXPECT_TRUE(
      ContainersEqual(expected_fragment0, fragments.begin()->view().data()));
  EXPECT_TRUE(ContainersEqual(expected_fragment1,
                              (++fragments.begin())->view().data()));
}

TEST(L2CAP_FragmenterTest, TwoFragmentsExact) {
  auto payload = CreateStaticByteBuffer('T', 'e', 's', 't');
  ZX_DEBUG_ASSERT_MSG(payload.size() % 2 == 0,
                      "test payload size should be even");

  auto expected_fragment0 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x00, 0x04, 0x00,

      // Basic L2CAP header, contains the complete length but a partial payload
      0x04, 0x00, 0x01, 0x00);

  auto expected_fragment1 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x10, 0x04, 0x00,

      // Continuing payload
      'T', 'e', 's', 't');

  // Make the fragment limit large enough to fit exactly half a B-frame
  // containing |payload|. The frame should be evenly divided across two
  // fragments.
  Fragmenter fragmenter(kTestHandle,
                        (payload.size() + sizeof(BasicHeader)) / 2);
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_EQ(2u, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();

  EXPECT_TRUE(
      ContainersEqual(expected_fragment0, fragments.begin()->view().data()));
  EXPECT_TRUE(ContainersEqual(expected_fragment1,
                              (++fragments.begin())->view().data()));
}

TEST(L2CAP_FragmenterTest, ManyFragmentsOffByOne) {
  constexpr size_t kMaxFragmentPayloadSize = 5;
  constexpr size_t kExpectedFragmentCount = 4;
  constexpr size_t kFrameSize =
      (kExpectedFragmentCount - 1) * kMaxFragmentPayloadSize + 1;

  StaticByteBuffer<kFrameSize - sizeof(BasicHeader)> payload;
  payload.Fill('X');

  auto expected_fragment0 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x00, 0x05, 0x00,

      // Basic L2CAP header contains the complete length but partial payload
      0x0C, 0x00, 0x01, 0x00, 'X');

  auto expected_fragment1 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x10, 0x05, 0x00,

      // Continuing payload
      'X', 'X', 'X', 'X', 'X');

  auto expected_fragment2 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x10, 0x05, 0x00,

      // Continuing payload
      'X', 'X', 'X', 'X', 'X');

  auto expected_fragment3 = CreateStaticByteBuffer(
      // ACL data header
      0x01, 0x10, 0x01, 0x00,

      // Continuing payload
      'X');

  Fragmenter fragmenter(kTestHandle, kMaxFragmentPayloadSize);
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_EQ(kExpectedFragmentCount, pdu.fragment_count());

  auto fragments = pdu.ReleaseFragments();
  auto iter = fragments.begin();
  EXPECT_TRUE(ContainersEqual(expected_fragment0, (iter++)->view().data()));
  EXPECT_TRUE(ContainersEqual(expected_fragment1, (iter++)->view().data()));
  EXPECT_TRUE(ContainersEqual(expected_fragment2, (iter++)->view().data()));
  EXPECT_TRUE(ContainersEqual(expected_fragment3, iter->view().data()));
}

TEST(L2CAP_FragmenterTest, MaximalSizedPayload) {
  DynamicByteBuffer payload(65535);
  Fragmenter fragmenter(kTestHandle, 1024);
  PDU pdu = fragmenter.BuildBasicFrame(kTestChannelId, payload);
  ASSERT_TRUE(pdu.is_valid());
  EXPECT_LT(64u, pdu.fragment_count());
}

}  // namespace
}  // namespace l2cap
}  // namespace bt
