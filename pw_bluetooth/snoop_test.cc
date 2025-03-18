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

#include "pw_bluetooth/snoop.h"

#include <vector>

#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bytes/span.h"
#include "pw_chrono/simulated_system_clock.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth {
namespace {
using ::std::chrono_literals::operator""us;

constexpr uint8_t hex_char_to_int(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else {
    // Handle invalid characters as needed
    return 0;
  }
}

std::vector<std::byte> hex_string_to_bytes(std::string_view hex_str) {
  std::vector<std::byte> bytes{};
  uint8_t value = 0;
  for (size_t i = 0; i < hex_str.size(); i++) {
    if (i % 2 == 0) {
      value = hex_char_to_int(hex_str[i]) << 4;
    } else {
      value |= hex_char_to_int(hex_str[i]);
      bytes.push_back(std::byte(value));
    }
  }
  return bytes;
}

std::vector<std::byte> get_snoop_log(Snoop& snoop) {
  std::vector<std::byte> snoop_data;
  Status status = snoop.Dump([&snoop_data](ConstByteSpan data) {
    for (const std::byte item : data) {
      snoop_data.push_back(item);
    }
    return OkStatus();
  });
  EXPECT_TRUE(status.ok());
  return snoop_data;
}
}  // namespace

TEST(SnoopTest, HeaderOnly) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};

  std::string_view expected_snoop_data =  // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderTx) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> tx_data = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet{emboss::H4PacketType::ACL_DATA, tx_data};
  snoop.AddTx(packet);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000006"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456789a";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderTxTruncated) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 3> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> tx_data = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet{emboss::H4PacketType::ACL_DATA, tx_data};
  snoop.AddTx(packet);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderRx) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> rx_data = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet{emboss::H4PacketType::ACL_DATA, rx_data};
  snoop.AddRx(packet);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000006"
      // Packet Flags (32-bit)
      "00000001"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456789a";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderRxTruncated) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 3> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> rx_data = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet{emboss::H4PacketType::ACL_DATA, rx_data};
  snoop.AddRx(packet);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000001"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderTxTx) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> tx_data1 = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet1{emboss::H4PacketType::ACL_DATA, tx_data1};
  snoop.AddTx(packet1);

  // Add packet 2
  clock.AdvanceTime(pw::chrono::SystemClock::for_at_least(1us));
  std::array<uint8_t, 3> tx_data2 = {0xBC, 0xDE, 0xF0};
  proxy::H4PacketWithHci packet2{emboss::H4PacketType::COMMAND, tx_data2};
  snoop.AddTx(packet2);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000006"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456789a"
      // Packet 2
      // Original Length (32-bit)
      "00000004"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000001"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "01"
      // Packet Data[1-N] - Data
      "BCDEF0";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderRxRx) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> rx_data1 = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet1{emboss::H4PacketType::ACL_DATA, rx_data1};
  snoop.AddRx(packet1);

  // Add packet 2
  clock.AdvanceTime(pw::chrono::SystemClock::for_at_least(1us));
  std::array<uint8_t, 3> rx_data2 = {0xBC, 0xDE, 0xF0};
  proxy::H4PacketWithHci packet2{emboss::H4PacketType::COMMAND, rx_data2};
  snoop.AddRx(packet2);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000006"
      // Packet Flags (32-bit)
      "00000001"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456789a"
      // Packet 2
      // Original Length (32-bit)
      "00000004"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000001"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000001"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "01"
      // Packet Data[1-N] - Data
      "BCDEF0";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, HeaderRxTxRxTx) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};

  // Add packet 1
  std::array<uint8_t, 5> rx_data1 = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet1{emboss::H4PacketType::ACL_DATA, rx_data1};
  snoop.AddRx(packet1);

  // Add packet 2
  clock.AdvanceTime(pw::chrono::SystemClock::for_at_least(1us));
  std::array<uint8_t, 3> tx_data1 = {0xBC, 0xDE, 0xF0};
  proxy::H4PacketWithHci packet2{emboss::H4PacketType::COMMAND, tx_data1};
  snoop.AddTx(packet2);

  // Add packet 3
  clock.AdvanceTime(pw::chrono::SystemClock::for_at_least(1us));
  std::array<uint8_t, 5> rx_data2 = {0x21, 0x43, 0x65, 0x87, 0xA9};
  proxy::H4PacketWithHci packet3{emboss::H4PacketType::ACL_DATA, rx_data2};
  snoop.AddRx(packet3);

  // Add packet 4
  clock.AdvanceTime(pw::chrono::SystemClock::for_at_least(1us));
  std::array<uint8_t, 3> tx_data2 = {0xCB, 0xED, 0x0F};
  proxy::H4PacketWithHci packet4{emboss::H4PacketType::COMMAND, tx_data2};
  snoop.AddTx(packet4);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 1
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000006"
      // Packet Flags (32-bit)
      "00000001"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000000"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "123456789a"
      // Packet 2
      // Original Length (32-bit)
      "00000004"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000001"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "01"
      // Packet Data[1-N] - Data
      "BCDEF0"
      // Packet 3
      // Original Length (32-bit)
      "00000006"
      // Included Length (32-bit)
      "00000006"
      // Packet Flags (32-bit)
      "00000001"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000002"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "02"
      // Packet Data[1-N] - Data
      "21436587a9"
      // Packet 4
      // Original Length (32-bit)
      "00000004"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000003"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "01"
      // Packet Data[1-N] - Data
      "CBED0F";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, Disabled) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};
  EXPECT_TRUE(snoop.IsEnabled());

  // Disable
  snoop.Disable();
  EXPECT_FALSE(snoop.IsEnabled());

  // Add packet 1
  std::array<uint8_t, 5> rx_data1 = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet1{emboss::H4PacketType::ACL_DATA, rx_data1};
  snoop.AddRx(packet1);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

TEST(SnoopTest, DisabledEnable) {
  chrono::SimulatedSystemClock clock{};
  SnoopBuffer<4096, 256> snoop{clock};
  EXPECT_TRUE(snoop.IsEnabled());

  // Disable
  snoop.Disable();
  EXPECT_FALSE(snoop.IsEnabled());

  // Add packet 1
  std::array<uint8_t, 5> rx_data1 = {0x12, 0x34, 0x56, 0x78, 0x9A};
  proxy::H4PacketWithHci packet1{emboss::H4PacketType::ACL_DATA, rx_data1};
  snoop.AddRx(packet1);

  // Renable
  snoop.Enable();
  EXPECT_TRUE(snoop.IsEnabled());

  // Add packet 2
  clock.AdvanceTime(pw::chrono::SystemClock::for_at_least(1us));
  std::array<uint8_t, 3> tx_data1 = {0xBC, 0xDE, 0xF0};
  proxy::H4PacketWithHci packet2{emboss::H4PacketType::COMMAND, tx_data1};
  snoop.AddTx(packet2);

  // Validate
  std::string_view expected_snoop_data =
      ""
      // Identification Pattern (64-bit)
      "6274736e6f6f7000"
      // Version Number (32-bit)
      "00000001"
      // Datalink Type (32-bit)
      "000003ea"
      // Packet 2
      // Original Length (32-bit)
      "00000004"
      // Included Length (32-bit)
      "00000004"
      // Packet Flags (32-bit)
      "00000000"
      // Cumulative Drops (32-bit)
      "00000000"
      // Timestamp Microseconds (64-bit)
      "0000000000000001"
      // Packet Data[0] - HCI_TYPE (8-bit)
      "01"
      // Packet Data[1-N] - Data
      "BCDEF0";
  EXPECT_EQ(hex_string_to_bytes(expected_snoop_data), get_snoop_log(snoop));
}

}  // namespace pw::bluetooth
