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

class IsoInboundPacketAssemblerTest : public ::testing::Test {
 public:
  IsoInboundPacketAssembler* assembler() { return &assembler_; }
  std::queue<std::vector<std::byte>>* outgoing_packets() {
    return &outgoing_packets_;
  }

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
    std::unique_ptr<std::vector<uint8_t>> sdu_data =
        testing::GenDataBlob(sdu_fragment_size, /*starting_value=*/42);
    DynamicByteBuffer incoming_packet = testing::IsoDataPacket(
        /*connection_handle=*/123,
        pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*time_stamp=*/std::nullopt,
        /*packet_sequence_number=*/456,
        /*iso_sdu_length=*/sdu_fragment_size,
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        *sdu_data);

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

}  // namespace bt::iso
