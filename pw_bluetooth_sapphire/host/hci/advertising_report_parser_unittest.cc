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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_report_parser.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"

namespace bt::hci::test {
namespace {

TEST(AdvertisingReportParserTest, EmptyReport) {
  StaticByteBuffer bytes(0x3E, 0x02, 0x02, 0x00);

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  hci::AdvertisingReportParser parser(*event);
  EXPECT_FALSE(parser.HasMoreReports());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));
}

TEST(AdvertisingReportParserTest, SingleReportMalformed) {
  // clang-format off

  StaticByteBuffer bytes(
      0x3E, 0x0B, 0x02, 0x01,  // HCI event header and LE Meta Event params
      0x03, 0x02,              // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x00                                 // |length_data|. RSSI is missing
      );

  // clang-format on

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  hci::AdvertisingReportParser parser(*event);
  EXPECT_TRUE(parser.HasMoreReports());
  EXPECT_FALSE(parser.encountered_error());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));
  EXPECT_TRUE(parser.encountered_error());
}

TEST(AdvertisingReportParserTest, SingleReportNoData) {
  // clang-format off

  StaticByteBuffer bytes(
      0x3E, 0x0C, 0x02, 0x01,  // HCI event header and LE Meta Event params
      0x03, 0x02,              // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x00, 0x7F                           // |length_data|, RSSI
      );

  // clang-format on

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  AdvertisingReportParser parser(*event);
  EXPECT_TRUE(parser.HasMoreReports());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvNonConnInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kPublicIdentity, data->address_type);
  EXPECT_EQ("06:05:04:03:02:01", data->address.ToString());
  EXPECT_EQ(0u, data->length_data);
  EXPECT_EQ(hci_spec::kRSSIInvalid, rssi);

  // No other reports
  EXPECT_FALSE(parser.HasMoreReports());
  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));

  EXPECT_FALSE(parser.encountered_error());
}

TEST(AdvertisingReportParserTest, ReportsValidInvalid) {
  // clang-format off

  StaticByteBuffer bytes(
      0x3E, 0x16, 0x02, 0x02,  // HCI event header and LE Meta Event params
      0x03, 0x02,              // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x00, 0x7F,                          // |length_data|, RSSI
      0x03, 0x02,                          // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x0A, 0x7F                           // malformed |length_data|, RSSI
      );

  // clang-format on

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  AdvertisingReportParser parser(*event);
  EXPECT_TRUE(parser.HasMoreReports());
  EXPECT_FALSE(parser.encountered_error());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvNonConnInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kPublicIdentity, data->address_type);
  EXPECT_EQ("06:05:04:03:02:01", data->address.ToString());
  EXPECT_EQ(0u, data->length_data);
  EXPECT_EQ(hci_spec::kRSSIInvalid, rssi);

  // There are more reports...
  EXPECT_TRUE(parser.HasMoreReports());
  EXPECT_FALSE(parser.encountered_error());

  // ...but the report is malformed.
  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));
  EXPECT_TRUE(parser.encountered_error());
}

TEST(AdvertisingReportParserTest, ReportsAllValid) {
  // clang-format off

  StaticByteBuffer bytes(
      0x3E, 0x28, 0x02, 0x03,              // HCI event header and LE Meta Event params
      0x03, 0x02,                          // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x00, 0x7F,                          // |length_data|, RSSI

      0x00, 0x01,                          // event_type, address_type
      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,  // address
      0x03, 0x01, 0x02, 0x03, 0x0F,        // |length_data|, data, RSSI

      0x01, 0x00,                          // event_type, address_type
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,  // address
      0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // |length_data|, data
      0x01                                 // RSSI
      );

  // clang-format on

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  AdvertisingReportParser parser(*event);
  EXPECT_TRUE(parser.HasMoreReports());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvNonConnInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kPublicIdentity, data->address_type);
  EXPECT_EQ("06:05:04:03:02:01", data->address.ToString());
  EXPECT_EQ(0u, data->length_data);
  EXPECT_EQ(hci_spec::kRSSIInvalid, rssi);

  // There are more reports
  EXPECT_TRUE(parser.HasMoreReports());
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kRandom, data->address_type);
  EXPECT_EQ("0C:0B:0A:09:08:07", data->address.ToString());
  EXPECT_EQ(3, data->length_data);
  EXPECT_TRUE(ContainersEqual(std::array<uint8_t, 3>{{0x01, 0x02, 0x03}},
                              data->data,
                              data->length_data));
  EXPECT_EQ(15, rssi);

  // There are more reports
  EXPECT_TRUE(parser.HasMoreReports());
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvDirectInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kPublic, data->address_type);
  EXPECT_EQ("12:11:10:0F:0E:0D", data->address.ToString());
  EXPECT_EQ(5, data->length_data);
  EXPECT_TRUE(
      ContainersEqual(std::array<uint8_t, 5>{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
                      data->data,
                      data->length_data));
  EXPECT_EQ(1, rssi);

  // No more reports.
  EXPECT_FALSE(parser.HasMoreReports());
  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));

  EXPECT_FALSE(parser.encountered_error());
}

TEST(AdvertisingReportParserTest, ReportCountLessThanPayloadSize) {
  // clang-format off

  StaticByteBuffer bytes(
      0x3E, 0x28, 0x02,  // HCI event header and LE Meta Event param
      0x01,              // Event count is 1, even though packet contains 3
      0x03, 0x02,        // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x00, 0x7F,                          // |length_data|, RSSI

      0x00, 0x01,                          // event_type, address_type
      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,  // address
      0x03, 0x01, 0x02, 0x03, 0x0F,        // |length_data|, data, RSSI

      0x01, 0x00,                          // event_type, address_type
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,  // address
      0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // |length_data|, data
      0x01                                 // RSSI
      );

  // clang-format on

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  AdvertisingReportParser parser(*event);
  EXPECT_TRUE(parser.HasMoreReports());
  EXPECT_FALSE(parser.encountered_error());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvNonConnInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kPublicIdentity, data->address_type);
  EXPECT_EQ("06:05:04:03:02:01", data->address.ToString());
  EXPECT_EQ(0u, data->length_data);
  EXPECT_EQ(hci_spec::kRSSIInvalid, rssi);

  // Since the packet is malformed (the event payload contains 3 reports while
  // the header states there is only 1) this should return false.
  EXPECT_FALSE(parser.encountered_error());
  EXPECT_FALSE(parser.HasMoreReports());
  EXPECT_TRUE(parser.encountered_error());

  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));
  EXPECT_TRUE(parser.encountered_error());
}

TEST(AdvertisingReportParserTest, ReportCountGreaterThanPayloadSize) {
  // clang-format off

  StaticByteBuffer bytes(
      0x3E, 0x0C, 0x02,  // HCI event header and LE Meta Event param
      0x02,              // Event count is 2, even though packet contains 1
      0x03, 0x02,        // event_type, address_type
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // address
      0x00, 0x7F                           // |length_data|, RSSI
      );

  // clang-format on

  auto event = EventPacket::New(bytes.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(bytes);
  event->InitializeFromBuffer();

  AdvertisingReportParser parser(*event);
  EXPECT_TRUE(parser.HasMoreReports());

  const hci_spec::LEAdvertisingReportData* data;
  int8_t rssi;
  EXPECT_TRUE(parser.GetNextReport(&data, &rssi));
  EXPECT_EQ(hci_spec::LEAdvertisingEventType::kAdvNonConnInd, data->event_type);
  EXPECT_EQ(hci_spec::LEAddressType::kPublicIdentity, data->address_type);
  EXPECT_EQ("06:05:04:03:02:01", data->address.ToString());
  EXPECT_EQ(0u, data->length_data);
  EXPECT_EQ(hci_spec::kRSSIInvalid, rssi);

  EXPECT_FALSE(parser.encountered_error());

  // Since the packet is malformed (the event payload contains 1 report while
  // the header states there are 2) this should return false.
  EXPECT_FALSE(parser.HasMoreReports());
  EXPECT_FALSE(parser.GetNextReport(&data, &rssi));

  EXPECT_TRUE(parser.encountered_error());
}

}  // namespace
}  // namespace bt::hci::test
