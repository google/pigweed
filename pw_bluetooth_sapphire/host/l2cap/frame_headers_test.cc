// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/l2cap/frame_headers.h"

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_unit_test/framework.h"

namespace bt::l2cap::internal {
namespace {

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesInformationFrame) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_TRUE(StaticByteBuffer(0b0000'0000, 0)
                  .To<EnhancedControlField>()
                  .designates_information_frame());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesNonInformationFrame) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(StaticByteBuffer(0b0000'0001, 0)
                   .To<EnhancedControlField>()
                   .designates_information_frame());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesSupervisoryFrame) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_TRUE(StaticByteBuffer(0b00000001, 0)
                  .To<EnhancedControlField>()
                  .designates_supervisory_frame());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesNonSupervisoryFrame) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(StaticByteBuffer(0b00000000, 1)
                   .To<EnhancedControlField>()
                   .designates_supervisory_frame());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesStartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_TRUE(StaticByteBuffer(0, 0b01000000)
                  .To<EnhancedControlField>()
                  .designates_start_of_segmented_sdu());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesNonStartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_FALSE(StaticByteBuffer(0, 0b00000000)
                   .To<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
  EXPECT_FALSE(StaticByteBuffer(0, 0b10000000)
                   .To<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
  EXPECT_FALSE(StaticByteBuffer(0, 0b11000000)
                   .To<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
  EXPECT_FALSE(StaticByteBuffer(1, 0b01000000)
                   .To<EnhancedControlField>()
                   .designates_start_of_segmented_sdu());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesPartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_TRUE(StaticByteBuffer(0, 0b01000000)
                  .To<EnhancedControlField>()
                  .designates_part_of_segmented_sdu());
  EXPECT_TRUE(StaticByteBuffer(0, 0b10000000)
                  .To<EnhancedControlField>()
                  .designates_part_of_segmented_sdu());
  EXPECT_TRUE(StaticByteBuffer(0, 0b11000000)
                  .To<EnhancedControlField>()
                  .designates_part_of_segmented_sdu());
}

TEST(FrameHeadersEnhancedControlFieldTest, IdentifiesNotPartOfSegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.
  EXPECT_FALSE(StaticByteBuffer(0, 0b00000000)
                   .To<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
  EXPECT_FALSE(StaticByteBuffer(1, 0b01000000)
                   .To<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
  EXPECT_FALSE(StaticByteBuffer(1, 0b10000000)
                   .To<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
  EXPECT_FALSE(StaticByteBuffer(1, 0b11000000)
                   .To<EnhancedControlField>()
                   .designates_part_of_segmented_sdu());
}

TEST(FrameHeadersEnhancedControlFieldTest, ReadsRequestSequenceNumber) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2, and Core Spec v5, Vol 3, Part
  // A, Sec 8.3.
  for (uint8_t seq_num = 0; seq_num < 64; ++seq_num) {
    EXPECT_EQ(seq_num,
              StaticByteBuffer(0, seq_num)
                  .To<EnhancedControlField>()
                  .receive_seq_num());
  }
}

TEST(FrameHeadersEnhancedControlFieldTest, IsConstructedProperly) {
  EnhancedControlField ecf;
  EXPECT_EQ(StaticByteBuffer(0, 0), BufferView(&ecf, sizeof(ecf)));
}

TEST(FrameHeadersEnhancedControlFieldTest,
     SetSupervisoryFrameSetsBitCorrectly) {
  EnhancedControlField ecf;
  ecf.set_supervisory_frame();
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(StaticByteBuffer(0b1, 0), BufferView(&ecf, sizeof(ecf)));
}

TEST(FrameHeadersEnhancedControlFieldTest, SetRequestSeqNumSetsBitsCorrectly) {
  for (uint8_t seq_num = 0; seq_num < 64; ++seq_num) {
    EnhancedControlField ecf;
    ecf.set_receive_seq_num(seq_num);
    // See Core Spec, v5, Vol 3, Part A, Table 3.2.
    EXPECT_EQ(StaticByteBuffer(0, seq_num), BufferView(&ecf, sizeof(ecf)));
  }
}

TEST(FrameHeadersEnhancedControlFieldTest,
     SetSegmentationStatusWorksCorrectlyOnFreshFrame) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::Unsegmented);
    EXPECT_EQ(StaticByteBuffer(0, 0), BufferView(&ecf, sizeof(ecf)));
  }

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::FirstSegment);
    EXPECT_EQ(StaticByteBuffer(0, 0b0100'0000), BufferView(&ecf, sizeof(ecf)));
  }

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::LastSegment);
    EXPECT_EQ(StaticByteBuffer(0, 0b1000'0000), BufferView(&ecf, sizeof(ecf)));
  }

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::MiddleSegment);
    EXPECT_EQ(StaticByteBuffer(0, 0b1100'0000), BufferView(&ecf, sizeof(ecf)));
  }
}

TEST(FrameHeadersEnhancedControlFieldTest,
     SetSegmentationStatusWorksCorrectlyOnRecycledFrame) {
  // See Core Spec, v5, Vol 3, Part A, Tables 3.2 and 3.4.

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::MiddleSegment);
    ecf.set_segmentation_status(SegmentationStatus::Unsegmented);
    EXPECT_EQ(StaticByteBuffer(0, 0), BufferView(&ecf, sizeof(ecf)));
  }

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::MiddleSegment);
    ecf.set_segmentation_status(SegmentationStatus::FirstSegment);
    EXPECT_EQ(StaticByteBuffer(0, 0b0100'0000), BufferView(&ecf, sizeof(ecf)));
  }

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::MiddleSegment);
    ecf.set_segmentation_status(SegmentationStatus::LastSegment);
    EXPECT_EQ(StaticByteBuffer(0, 0b1000'0000), BufferView(&ecf, sizeof(ecf)));
  }

  {
    EnhancedControlField ecf;
    ecf.set_segmentation_status(SegmentationStatus::MiddleSegment);
    ecf.set_segmentation_status(SegmentationStatus::MiddleSegment);
    EXPECT_EQ(StaticByteBuffer(0, 0b1100'0000), BufferView(&ecf, sizeof(ecf)));
  }
}

TEST(FrameHeadersEnhancedControlFieldTest,
     SetSegmentationStatusPreservesRequestSeqNum) {
  EnhancedControlField ecf;
  ecf.set_receive_seq_num(EnhancedControlField::kMaxSeqNum);
  ecf.set_segmentation_status(SegmentationStatus::Unsegmented);
  EXPECT_EQ(EnhancedControlField::kMaxSeqNum, ecf.receive_seq_num());
}

TEST(FrameHeadersSimpleInformationFrameHeaderTest, ReadsTxSequenceNumber) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2, and Core Spec v5, Vol 3, Part
  // A, Sec 8.3.
  for (uint8_t seq_num = 0; seq_num < 64; ++seq_num) {
    EXPECT_EQ(seq_num,
              StaticByteBuffer(seq_num << 1, 0)
                  .To<SimpleInformationFrameHeader>()
                  .tx_seq());
  }
}

TEST(FrameHeadersSimpleInformationFrameHeaderTest, IsConstructedProperly) {
  constexpr uint8_t kTxSeq = 63;
  SimpleInformationFrameHeader frame(kTxSeq);
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(StaticByteBuffer(0b111'1110, 0), BufferView(&frame, sizeof(frame)));
}

TEST(FrameHeadersSimpleInformationFrameHeaderTest,
     SetSegmentationStatusPreservesTxSeq) {
  constexpr uint8_t kTxSeq = 63;
  SimpleInformationFrameHeader frame(kTxSeq);
  frame.set_segmentation_status(SegmentationStatus::Unsegmented);
  EXPECT_EQ(kTxSeq, frame.tx_seq());
}

TEST(FrameHeadersSimpleStartOfSduFrameHeaderTest, IsConstructedProperly) {
  constexpr uint8_t kTxSeq = 63;
  SimpleStartOfSduFrameHeader frame(kTxSeq);
  // See Core Spec, v5, Vol 3, Part A, Table 3.2, and Figure 3.3.
  EXPECT_EQ(StaticByteBuffer(0b111'1110, 0b0100'0000, 0, 0),
            BufferView(&frame, sizeof(frame)));
}

TEST(FrameHeadersSimpleSupervisoryFrameTest, IsConstructedProperly) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::ReceiverReady);
    EXPECT_EQ(StaticByteBuffer(0b0001, 0), BufferView(&frame, sizeof(frame)));
  }

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::Reject);
    EXPECT_EQ(StaticByteBuffer(0b0101, 0), BufferView(&frame, sizeof(frame)));
  }

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::ReceiverNotReady);
    EXPECT_EQ(StaticByteBuffer(0b1001, 0), BufferView(&frame, sizeof(frame)));
  }

  {
    SimpleSupervisoryFrame frame(SupervisoryFunction::SelectiveReject);
    EXPECT_EQ(StaticByteBuffer(0b1101, 0), BufferView(&frame, sizeof(frame)));
  }
}

TEST(FrameHeadersSimpleSupervisoryFrameTest, IdentifiesPollRequest) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(StaticByteBuffer(0b0'0001, 0)
                   .To<SimpleSupervisoryFrame>()
                   .is_poll_request());
  EXPECT_TRUE(StaticByteBuffer(0b1'0001, 0)
                  .To<SimpleSupervisoryFrame>()
                  .is_poll_request());
}

TEST(FrameHeadersSimpleSupervisoryFrameTest, IdentifiesPollResponse) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_FALSE(StaticByteBuffer(0b0000'0001, 0)
                   .To<SimpleSupervisoryFrame>()
                   .is_poll_response());
  EXPECT_TRUE(StaticByteBuffer(0b1000'0001, 0)
                  .To<SimpleSupervisoryFrame>()
                  .is_poll_response());
}

TEST(FrameHeadersSimpleSupervisoryFrameTest, FunctionReadsSupervisoryFunction) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 and Table 3.5.
  EXPECT_EQ(
      SupervisoryFunction::ReceiverReady,
      StaticByteBuffer(0b0001, 0).To<SimpleSupervisoryFrame>().function());
  EXPECT_EQ(
      SupervisoryFunction::Reject,
      StaticByteBuffer(0b0101, 0).To<SimpleSupervisoryFrame>().function());
  EXPECT_EQ(
      SupervisoryFunction::ReceiverNotReady,
      StaticByteBuffer(0b1001, 0).To<SimpleSupervisoryFrame>().function());
  EXPECT_EQ(
      SupervisoryFunction::SelectiveReject,
      StaticByteBuffer(0b1101, 0).To<SimpleSupervisoryFrame>().function());
}

TEST(FrameHeadersSimpleSupervisoryFrameTest, SetIsPollRequestSetsCorrectBit) {
  SimpleSupervisoryFrame sframe(SupervisoryFunction::ReceiverReady);
  sframe.set_is_poll_request();
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(StaticByteBuffer(0b1'0001, 0), BufferView(&sframe, sizeof(sframe)));
}

TEST(FrameHeadersSimpleSupervisoryFrameTest, SetIsPollResponseSetsCorrectBit) {
  SimpleSupervisoryFrame sframe(SupervisoryFunction::ReceiverReady);
  sframe.set_is_poll_response();
  // See Core Spec, v5, Vol 3, Part A, Table 3.2.
  EXPECT_EQ(StaticByteBuffer(0b1000'0001, 0),
            BufferView(&sframe, sizeof(sframe)));
}

TEST(FrameHeadersSimpleReceiverReadyFrameTest, IsConstructedProperly) {
  SimpleReceiverReadyFrame frame;
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 and Table 3.5.
  EXPECT_EQ(StaticByteBuffer(0b0001, 0), BufferView(&frame, sizeof(frame)));
}

}  // namespace
}  // namespace bt::l2cap::internal
