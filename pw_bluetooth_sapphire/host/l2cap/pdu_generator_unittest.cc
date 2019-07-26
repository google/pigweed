// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdu_generator.h"

#include "gtest/gtest.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

TEST(L2CAP_PduGeneratorTest, CanGeneratePdusFromEvenlyDivisbleBuffer) {
  std::array<uint8_t, 1024> buf;
  internal::PduGenerator gen(buf.data(), buf.size(), 2);
  EXPECT_TRUE(gen.GetNextPdu());
  EXPECT_TRUE(gen.GetNextPdu());
}

TEST(L2CAP_PduGeneratorTest, ReadingPastEndFromEvenlyDivisibleBufferYieldsFalse) {
  std::array<uint8_t, 1024> buf;
  internal::PduGenerator gen(buf.data(), buf.size(), 2);
  gen.GetNextPdu();
  gen.GetNextPdu();
  EXPECT_FALSE(gen.GetNextPdu());
}

TEST(L2CAP_PduGeneratorTest, CanGeneratePdusFromNonEvenlyDivisibleBuffer) {
  std::array<uint8_t, 1024> buf;
  internal::PduGenerator gen(buf.data(), buf.size(), 3);
  for (size_t i = 0; i < 3; ++i) {
    auto pdu = gen.GetNextPdu();
    ASSERT_TRUE(pdu);
    EXPECT_EQ(341u, pdu.value().length());
  }
}

TEST(L2CAP_PduGeneratorTest, ReadingPastEndFromNonEvenlyDivisibleBufferYieldsFalse) {
  std::array<uint8_t, 1024> buf;
  internal::PduGenerator gen(buf.data(), buf.size(), 3);
  gen.GetNextPdu();
  gen.GetNextPdu();
  gen.GetNextPdu();
  EXPECT_FALSE(gen.GetNextPdu());
}

TEST(L2CAP_PduGeneratorTest, ZeroNumPdusYieldsZeroPdus) {
  std::array<uint8_t, 1024> buf;
  EXPECT_FALSE(internal::PduGenerator(buf.data(), buf.size(), 0).GetNextPdu());
}

TEST(L2CAP_PduGeneratorTest, NumPdusGreaterThanNumBytesYieldsZeroPdus) {
  std::array<uint8_t, 1024> buf;
  EXPECT_FALSE(internal::PduGenerator(buf.data(), buf.size(), buf.size() + 1).GetNextPdu());
}

TEST(L2CAP_PduGeneratorTest, ZeroBufSizeDoesNotCrash) {
  std::array<uint8_t, 1024> buf;
  internal::PduGenerator(buf.data(), 0, 1).GetNextPdu();
}

TEST(L2CAP_PduGeneratorTest, OversizedBufSizeDoesNotCrash) {
  std::array<uint8_t, 2 * kMaxBasicFramePayloadSize> buf;
  internal::PduGenerator(buf.data(), buf.size(), 1).GetNextPdu();
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
