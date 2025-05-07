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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_inbound_packet_assembler.h"

#include <gtest/gtest.h>
#include <pw_bluetooth/hci_data.emb.h>

#include <queue>

#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::iso {

constexpr uint16_t kDefaultConnectionHandle = 111;
constexpr uint16_t kDefaultPacketSequenceNumber = 456;

class IsoInboundPacketAssemblerTest : public ::testing::Test {
 public:
  IsoInboundPacketAssembler* assembler() { return &assembler_; }
  std::queue<std::vector<std::byte>>* outgoing_packets() {
    return &outgoing_packets_;
  }

 protected:
  bool TestFragmentedSdu(
      const std::vector<size_t>& fragment_sizes,
      std::optional<size_t> complete_sdu_size = std::nullopt);

 private:
  void HandleCompletePacket(const pw::span<const std::byte>& packet) {
    std::vector<std::byte> new_packet(packet.size());
    std::copy(packet.begin(), packet.end(), new_packet.begin());
    outgoing_packets_.emplace(std::move(new_packet));
  }

  std::queue<std::vector<std::byte>> outgoing_packets_;

  IsoInboundPacketAssembler assembler_{
      fit::bind_member<&IsoInboundPacketAssemblerTest::HandleCompletePacket>(
          this)};
};

// Verify that a complete packet is immediately passed to the handler
TEST_F(IsoInboundPacketAssemblerTest, CompleteSdu) {
  const size_t kFramesToBeSent = 12;
  size_t sdu_fragment_size = 100;
  const size_t kSubsequentSizeIncrement = 20;

  for (size_t frames_sent = 0; frames_sent < kFramesToBeSent; frames_sent++) {
    std::vector<uint8_t> sdu_data =
        testing::GenDataBlob(sdu_fragment_size, /*starting_value=*/42);
    DynamicByteBuffer incoming_packet = testing::IsoDataPacket(
        kDefaultConnectionHandle,
        pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*time_stamp=*/std::nullopt,
        kDefaultPacketSequenceNumber,
        /*iso_sdu_length=*/sdu_fragment_size,
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        sdu_data);

    ASSERT_EQ(outgoing_packets()->size(), frames_sent);
    pw::span<const std::byte> frame_as_span = incoming_packet.subspan();
    assembler()->ProcessNext(frame_as_span);
    EXPECT_EQ(outgoing_packets()->size(), frames_sent + 1);
    EXPECT_TRUE(std::equal(frame_as_span.begin(),
                           frame_as_span.end(),
                           outgoing_packets()->back().begin(),
                           outgoing_packets()->back().end()));
    sdu_fragment_size += kSubsequentSizeIncrement;
  }
}

bool IsoInboundPacketAssemblerTest::TestFragmentedSdu(
    const std::vector<size_t>& fragment_sizes,
    std::optional<size_t> complete_sdu_size) {
  size_t initial_frames_received = outgoing_packets()->size();

  // Populate our SDU backing data buffer
  size_t data_size = 0;
  for (size_t fragment_size : fragment_sizes) {
    data_size += fragment_size;
  }
  std::vector<uint8_t> sdu =
      testing::GenDataBlob(data_size, /*starting_value=*/76);

  std::vector<DynamicByteBuffer> iso_data_fragment_packets =
      testing::IsoDataFragments(
          kDefaultConnectionHandle,
          /*time_stamp=*/std::nullopt,
          kDefaultPacketSequenceNumber,
          pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
          sdu,
          fragment_sizes);

  // Override the iso_sdu_length field, if an alternate value was provided
  if (complete_sdu_size.has_value() && ((*complete_sdu_size) != data_size)) {
    PW_CHECK(iso_data_fragment_packets.size() > 0);
    auto view = pw::bluetooth::emboss::MakeIsoDataFramePacketView(
        iso_data_fragment_packets[0].mutable_data(),
        iso_data_fragment_packets[0].size());
    PW_CHECK((view.header().pb_flag().Read() ==
              pw::bluetooth::emboss::IsoDataPbFlag::FIRST_FRAGMENT) ||
             (view.header().pb_flag().Read() ==
              pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU));
    view.iso_sdu_length().Write(*complete_sdu_size);
  }

  for (size_t frames_sent = 0; frames_sent < fragment_sizes.size();
       frames_sent++) {
    // We should not receive any packets until all of the fragments have been
    // sent
    if (initial_frames_received != outgoing_packets()->size()) {
      return false;
    }
    pw::span<const std::byte> frame_as_span =
        iso_data_fragment_packets[frames_sent].subspan();
    assembler()->ProcessNext(frame_as_span);
  }
  if (outgoing_packets()->size() != (initial_frames_received + 1)) {
    return false;
  }

  // The output should look the same as if we had constructed a non-fragmented
  // packet ourselves. Verify that.
  DynamicByteBuffer expected_output = testing::IsoDataPacket(
      kDefaultConnectionHandle,
      pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
      /*time_stamp=*/std::nullopt,
      kDefaultPacketSequenceNumber,
      data_size,
      pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
      sdu);
  pw::span<const std::byte> expected_output_as_span = expected_output.subspan();
  return std::equal(expected_output_as_span.begin(),
                    expected_output_as_span.end(),
                    outgoing_packets()->back().begin(),
                    outgoing_packets()->back().end());
}

// FIRST_FRAGMENT + LAST_FRAGMENT
TEST_F(IsoInboundPacketAssemblerTest, TwoSduFragments) {
  std::vector<size_t> fragment_sizes = {100, 125};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes));
}

TEST_F(IsoInboundPacketAssemblerTest, OneIntermediateSduFragment) {
  std::vector<size_t> fragment_sizes = {100, 125, 150};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes));
}

TEST_F(IsoInboundPacketAssemblerTest, MultipleIntermediateSduFragments) {
  std::vector<size_t> fragment_sizes = {100, 125, 250, 500, 25};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes));
}

TEST_F(IsoInboundPacketAssemblerTest, MultipleTinySduFragments) {
  std::vector<size_t> fragment_sizes = {1, 1, 1, 1, 1, 1, 1, 1};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes));
}

// LAST_FRAGMENT takes us over the total SDU size -- packet should be discarded
TEST_F(IsoInboundPacketAssemblerTest, LastFragmentSduTooLong) {
  std::vector<size_t> fragment_sizes = {100, 125, 250, 500, 26};
  ASSERT_FALSE(TestFragmentedSdu(fragment_sizes, 1000));
  EXPECT_EQ(outgoing_packets()->size(), 0u);
}

// INTERMEDIATE_FRAGMENT takes us over the total SDU size -- packet should be
// discarded
TEST_F(IsoInboundPacketAssemblerTest, IntermediateFragmentSduTooLong) {
  std::vector<size_t> fragment_sizes = {100, 125, 250, 526, 100};
  ASSERT_FALSE(TestFragmentedSdu(fragment_sizes, 1000));
  EXPECT_EQ(outgoing_packets()->size(), 0u);
}

TEST_F(IsoInboundPacketAssemblerTest,
       NextSduReceivedBeforePreviousOneComplete) {
  constexpr uint16_t kCompleteSduSize = 375;
  std::vector<size_t> fragment_sizes = {125, 125, 125};

  std::vector<uint8_t> sdu =
      testing::GenDataBlob(kCompleteSduSize, /*starting_value=*/202);
  std::vector<DynamicByteBuffer> iso_data_fragment_packets =
      testing::IsoDataFragments(
          kDefaultConnectionHandle,
          /*time_stamp=*/std::nullopt,
          kDefaultPacketSequenceNumber,
          pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
          sdu,
          fragment_sizes);
  // Send all but the last frame
  for (size_t frames_sent = 0; frames_sent < (fragment_sizes.size() - 1);
       frames_sent++) {
    // We should not receive any packets until all of the fragments have been
    // sent
    ASSERT_EQ(outgoing_packets()->size(), 0u);

    pw::span<const std::byte> frame_as_span =
        iso_data_fragment_packets[frames_sent].subspan();
    assembler()->ProcessNext(frame_as_span);
  }
  ASSERT_EQ(outgoing_packets()->size(), 0u);

  // We should be able to send follow-up SDUs and the partially-received
  // one will have been dropped.
  std::vector<size_t> fragment_sizes_2 = {100, 125, 250, 500, 25};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes_2));
  std::vector<size_t> fragment_sizes_3 = {443};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes_3));
}

TEST_F(IsoInboundPacketAssemblerTest, UnexpectedIntermediateFragmentReceived) {
  std::vector<uint8_t> sdu_data =
      testing::GenDataBlob(/*size=*/100, /*starting_value=*/99);
  DynamicByteBuffer incoming_packet = testing::IsoDataPacket(
      kDefaultConnectionHandle,
      pw::bluetooth::emboss::IsoDataPbFlag::INTERMEDIATE_FRAGMENT,
      /*time_stamp=*/std::nullopt,
      /*packet_sequence_number=*/std::nullopt,
      /*iso_sdu_length=*/std::nullopt,
      /*packet_status_flag=*/std::nullopt,
      sdu_data);
  ASSERT_EQ(outgoing_packets()->size(), 0u);
  assembler()->ProcessNext(incoming_packet.subspan());

  // Nothing passed through
  ASSERT_EQ(outgoing_packets()->size(), 0u);

  // We should be able to send follow-up SDUs and the incomplete fragment will
  // have been dropped.
  std::vector<size_t> fragment_sizes_2 = {100, 125, 250, 500, 25};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes_2));
  std::vector<size_t> fragment_sizes_3 = {443};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes_3));
}

TEST_F(IsoInboundPacketAssemblerTest, UnexpectedLastFragmentReceived) {
  std::vector<uint8_t> sdu =
      testing::GenDataBlob(/*size=*/100, /*starting_value=*/99);
  DynamicByteBuffer incoming_packet = testing::IsoDataPacket(
      kDefaultConnectionHandle,
      pw::bluetooth::emboss::IsoDataPbFlag::LAST_FRAGMENT,
      /*time_stamp=*/std::nullopt,
      /*packet_sequence_number=*/std::nullopt,
      /*iso_sdu_length=*/std::nullopt,
      /*packet_status_flag=*/std::nullopt,
      sdu);
  ASSERT_EQ(outgoing_packets()->size(), 0u);
  assembler()->ProcessNext(incoming_packet.subspan());

  // Nothing passed through
  ASSERT_EQ(outgoing_packets()->size(), 0u);

  // We should be able to send follow-up SDUs and the incomplete fragment will
  // have been dropped.
  std::vector<size_t> fragment_sizes_2 = {100, 125, 250, 500, 25};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes_2));
  std::vector<size_t> fragment_sizes_3 = {443};
  ASSERT_TRUE(TestFragmentedSdu(fragment_sizes_3));
}

}  // namespace bt::iso
