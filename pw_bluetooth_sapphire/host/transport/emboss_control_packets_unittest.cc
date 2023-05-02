// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "emboss_control_packets.h"

#include <gtest/gtest.h>

namespace bt::hci {

const size_t BITS_IN_BYTE = 8;

TEST(EmbossControlPackets, TooSmallCommandCompleteEventWhenReadingStatus) {
  StaticByteBuffer buffer(hci_spec::kCommandCompleteEventCode,
                          0x03,       // size
                          0x01,       // num hci packets
                          0x02, 0x03  // command opcode
                                      // There should be a status field here, but there isn't
  );
  EmbossEventPacket packet = EmbossEventPacket::New(buffer.size());
  packet.mutable_data().Write(buffer);
  EXPECT_FALSE(packet.StatusCode().has_value());
}

TEST(EmbossControlPackets, CommandCompleteEventWithStatus) {
  StaticByteBuffer buffer(hci_spec::kCommandCompleteEventCode,
                          0x04,        // size
                          0x01,        // num hci packets
                          0x02, 0x03,  // command opcode
                          0x00         // status (success)
  );
  EmbossEventPacket packet = EmbossEventPacket::New(buffer.size());
  packet.mutable_data().Write(buffer);
  ASSERT_TRUE(packet.StatusCode().has_value());
  EXPECT_EQ(packet.StatusCode().value(), pw::bluetooth::emboss::StatusCode::SUCCESS);
}

TEST(EmbossControlPackets, ArrayFieldWithVariableLengthElements) {
  // Size of `data` arrays in the two Advertising Reports.
  constexpr size_t first_report_data_size = 3, second_report_data_size = 2;
  // Size of `reports` array in the Advertising Report Subevent.
  size_t reports_size =
      first_report_data_size + second_report_data_size +
      2ul * pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes();
  // Size of LEMetaEvent containing the Advertising Report Subevent.
  size_t packet_size =
      reports_size + pw::bluetooth::emboss::LEExtendedAdvertisingReportSubevent::MinSizeInBytes();

  auto packet =
      EmbossEventPacket::New<pw::bluetooth::emboss::LEExtendedAdvertisingReportSubeventWriter>(
          hci_spec::kLEMetaEventCode, packet_size);
  auto view = packet.view_t(reports_size);
  view.num_reports().Write(2);
  ASSERT_TRUE(view.Ok());
  EXPECT_EQ(view.reports().SizeInBytes(), reports_size);

  // Create view over the first Report in the `reports` array of the subevent.
  auto first_report_view = pw::bluetooth::emboss::MakeLEExtendedAdvertisingReportDataView(
      view.reports().BackingStorage().data(),
      pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes() +
          first_report_data_size);
  first_report_view.data_length().Write(first_report_data_size);
  first_report_view.data()
      .BackingStorage()
      .WriteBigEndianUInt<first_report_data_size * BITS_IN_BYTE>(0x123456);
  ASSERT_TRUE(first_report_view.Ok());
  EXPECT_EQ(first_report_view.data().SizeInBytes(), first_report_data_size);
  EXPECT_EQ(first_report_view.data()[0].Read(), 0x12);
  EXPECT_EQ(first_report_view.data()[1].Read(), 0x34);
  EXPECT_EQ(first_report_view.data()[2].Read(), 0x56);

  // Create view over the second Report in the `reports` array of the subevent.
  auto second_report_view = pw::bluetooth::emboss::MakeLEExtendedAdvertisingReportDataView(
      view.reports().BackingStorage().data() + first_report_data_size,
      pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes() +
          second_report_data_size);
  second_report_view.data_length().Write(second_report_data_size);
  second_report_view.data()
      .BackingStorage()
      .WriteBigEndianUInt<second_report_data_size * BITS_IN_BYTE>(0x1234);
  ASSERT_TRUE(second_report_view.Ok());
  EXPECT_EQ(second_report_view.data().SizeInBytes(), second_report_data_size);
  EXPECT_EQ(second_report_view.data()[0].Read(), 0x12);
  EXPECT_EQ(second_report_view.data()[1].Read(), 0x34);
}

}  // namespace bt::hci
