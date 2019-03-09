// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "l2cap_internal.h"

#include <iostream>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "gtest/gtest.h"

namespace btlib {
namespace l2cap {
namespace internal {
namespace {

using common::BufferView;
using common::CreateStaticByteBuffer;

TEST(L2CAP_Internal_EnhancedControlFieldTest, IdentifiesSupervisoryFrame) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_TRUE(CreateStaticByteBuffer(0b00000001, 0)
                  .As<EnhancedControlField>()
                  .designates_supervisory_frame());
}

TEST(L2CAP_Internal_EnhancedControlFieldTest, IdentifiesNonSupervisoryFrame) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(CreateStaticByteBuffer(0b00000000, 1)
                   .As<EnhancedControlField>()
                   .designates_supervisory_frame());
}

TEST(L2CAP_Internal_EnhancedControlFieldTest, IdentifiesStartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_TRUE(CreateStaticByteBuffer(0, 0b01000000)
                  .As<EnhancedControlField>()
                  .designates_start_of_segmented_sdu());
}

TEST(L2CAP_Internal_EnhancedControlFieldTest,
     IdentifiesNonStartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_FALSE(CreateStaticByteBuffer(0, 0b00000000)
                   .As<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
  EXPECT_FALSE(CreateStaticByteBuffer(0, 0b10000000)
                   .As<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
  EXPECT_FALSE(CreateStaticByteBuffer(0, 0b11000000)
                   .As<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
  EXPECT_FALSE(CreateStaticByteBuffer(1, 0b01000000)
                   .As<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
}

TEST(L2CAP_Internal_EnhancedControlFieldTest, IdentifiesPartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_TRUE(CreateStaticByteBuffer(0, 0b01000000)
                  .As<EnhancedControlField>()
                  .designates_part_of_segmented_sdu());
  EXPECT_TRUE(CreateStaticByteBuffer(0, 0b10000000)
                  .As<EnhancedControlField>()
                  .designates_part_of_segmented_sdu());
  EXPECT_TRUE(CreateStaticByteBuffer(0, 0b11000000)
                  .As<EnhancedControlField>()
                  .designates_part_of_segmented_sdu());
}

TEST(L2CAP_Internal_EnhancedControlFieldTest, IdentifiesNotPartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_FALSE(CreateStaticByteBuffer(0, 0b00000000)
                   .As<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
  EXPECT_FALSE(CreateStaticByteBuffer(1, 0b01000000)
                   .As<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
  EXPECT_FALSE(CreateStaticByteBuffer(1, 0b10000000)
                   .As<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
  EXPECT_FALSE(CreateStaticByteBuffer(1, 0b11000000)
                   .As<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
}

TEST(L2CAP_Internal_EnhancedControlFieldTest, ReadsRequestSequenceNumber) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2, and Core Spec v5, Vol 3, Part
  // A, Sec 8.3.
  for (uint8_t seq_num = 0; seq_num < 64; ++seq_num) {
    EXPECT_EQ(seq_num, CreateStaticByteBuffer(0, seq_num)
                           .As<EnhancedControlField>()
                           .request_seq_num());
  }
}

TEST(L2CAP_Internal_EnhancedControlFieldTest, IsConstructedProperly) {
  EnhancedControlField ecf;
  EXPECT_EQ(CreateStaticByteBuffer(0, 0), BufferView(&ecf, sizeof(ecf)));
}

TEST(L2CAP_Internal_EnhancedControlFieldTest,
     SetSupervisoryFrameSetsBitCorrectly) {
  EnhancedControlField ecf;
  ecf.set_supervisory_frame();
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(CreateStaticByteBuffer(0b1, 0), BufferView(&ecf, sizeof(ecf)));
}

TEST(L2CAP_Internal_EnhancedControlFieldTest,
     SetRequestSeqNumSetsBitsCorrectly) {
  for (uint8_t seq_num = 0; seq_num < 64; ++seq_num) {
    EnhancedControlField ecf;
    ecf.set_request_seq_num(seq_num);
    // See Core Spec, v5, Vol 3, Part A, Table 3.2.
    EXPECT_EQ(CreateStaticByteBuffer(0, seq_num),
              BufferView(&ecf, sizeof(ecf)));
  }
}

TEST(L2CAP_Internal_SimpleInformationFrameHeaderTest, ReadsTxSequenceNumber) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2, and Core Spec v5, Vol 3, Part
  // A, Sec 8.3.
  for (uint8_t seq_num = 0; seq_num < 64; ++seq_num) {
    EXPECT_EQ(seq_num, CreateStaticByteBuffer(seq_num << 1, 0)
                           .As<SimpleInformationFrameHeader>()
                           .tx_seq());
  }
}

TEST(L2CAP_Internal_SimpleStartOfSduFrameHeaderTest, IsConstructedProperly) {
  SimpleStartOfSduFrameHeader frame;
  // See Core Spec, v5, Vol 3, Part A, Table 3.2, and Figure 3.3.
  EXPECT_EQ(CreateStaticByteBuffer(0, 0, 0, 0),
            BufferView(&frame, sizeof(frame)));
}

TEST(L2CAP_Internal_SimpleSupervisoryFrameTest, IsConstructedProperly) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::ReceiverReady);
    EXPECT_EQ(CreateStaticByteBuffer(0b0001, 0),
              BufferView(&frame, sizeof(frame)));
  }

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::Reject);
    EXPECT_EQ(CreateStaticByteBuffer(0b0101, 0),
              BufferView(&frame, sizeof(frame)));
  }

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::ReceiverNotReady);
    EXPECT_EQ(CreateStaticByteBuffer(0b1001, 0),
              BufferView(&frame, sizeof(frame)));
  }

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::SelectiveReject);
    EXPECT_EQ(CreateStaticByteBuffer(0b1101, 0),
              BufferView(&frame, sizeof(frame)));
  }
}

TEST(L2CAP_Internal_SimpleSupervisoryFrameTest, IdentifiesPollRequest) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(CreateStaticByteBuffer(0b0'0001, 0)
                   .As<SimpleSupervisoryFrame>()
                   .is_poll_request());
  EXPECT_TRUE(CreateStaticByteBuffer(0b1'0001, 0)
                  .As<SimpleSupervisoryFrame>()
                  .is_poll_request());
}

TEST(L2CAP_Internal_SimpleSupervisoryFrameTest, IdentifiesPollResponse) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(CreateStaticByteBuffer(0b0000'0001, 0)
                   .As<SimpleSupervisoryFrame>()
                   .is_poll_response());
  EXPECT_TRUE(CreateStaticByteBuffer(0b1000'0001, 0)
                  .As<SimpleSupervisoryFrame>()
                  .is_poll_response());
}

TEST(L2CAP_Internal_SimpleSupervisoryFrameTest,
     FunctionReadsSupervisoryFunction) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 and Table 3.5.
  EXPECT_EQ(SupervisoryFunction::ReceiverReady,
            CreateStaticByteBuffer(0b0001, 0)
                .As<SimpleSupervisoryFrame>()
                .function());
  EXPECT_EQ(SupervisoryFunction::Reject, CreateStaticByteBuffer(0b0101, 0)
                                             .As<SimpleSupervisoryFrame>()
                                             .function());
  EXPECT_EQ(SupervisoryFunction::ReceiverNotReady,
            CreateStaticByteBuffer(0b1001, 0)
                .As<SimpleSupervisoryFrame>()
                .function());
  EXPECT_EQ(SupervisoryFunction::SelectiveReject,
            CreateStaticByteBuffer(0b1101, 0)
                .As<SimpleSupervisoryFrame>()
                .function());
}

TEST(L2CAP_Internal_SimpleSupervisoryFrameTest,
     SetIsPollRequestSetsCorrectBit) {
  SimpleSupervisoryFrame sframe(SupervisoryFunction::ReceiverReady);
  sframe.set_is_poll_request();
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(CreateStaticByteBuffer(0b1'0001, 0),
            BufferView(&sframe, sizeof(sframe)));
}

TEST(L2CAP_Internal_SimpleSupervisoryFrameTest,
     SetIsPollResponseSetsCorrectBit) {
  SimpleSupervisoryFrame sframe(SupervisoryFunction::ReceiverReady);
  sframe.set_is_poll_response();
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(CreateStaticByteBuffer(0b1000'0001, 0),
            BufferView(&sframe, sizeof(sframe)));
}

TEST(L2CAP_Internal_SimpleReceiverReadyFrameTest, IsConstructedProperly) {
  SimpleReceiverReadyFrame frame;
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 and Table 3.5.
  EXPECT_EQ(CreateStaticByteBuffer(0b0001, 0),
            BufferView(&frame, sizeof(frame)));
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace btlib
