// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

#include <gtest/gtest.h>

#include "adapter_test_fixture.h"
#include "fuchsia/bluetooth/control/cpp/fidl.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace fble = fuchsia::bluetooth::le;
namespace fbt = fuchsia::bluetooth;
namespace fsys = fuchsia::bluetooth::sys;

namespace bthost {
namespace fidl_helpers {
namespace {

TEST(FidlHelpersTest, HostErrorToFidl) {
  EXPECT_EQ(fsys::Error::FAILED, HostErrorToFidl(bt::HostError::kFailed));
  EXPECT_EQ(fsys::Error::TIMED_OUT, HostErrorToFidl(bt::HostError::kTimedOut));
  EXPECT_EQ(fsys::Error::INVALID_ARGUMENTS, HostErrorToFidl(bt::HostError::kInvalidParameters));
  EXPECT_EQ(fsys::Error::CANCELED, HostErrorToFidl(bt::HostError::kCanceled));
  EXPECT_EQ(fsys::Error::IN_PROGRESS, HostErrorToFidl(bt::HostError::kInProgress));
  EXPECT_EQ(fsys::Error::NOT_SUPPORTED, HostErrorToFidl(bt::HostError::kNotSupported));
  EXPECT_EQ(fsys::Error::PEER_NOT_FOUND, HostErrorToFidl(bt::HostError::kNotFound));

  // All other errors currently map to FAILED.
  EXPECT_EQ(fsys::Error::FAILED, HostErrorToFidl(bt::HostError::kProtocolError));
}

TEST(FIDL_HelpersTest, AddressBytesFrommString) {
  EXPECT_FALSE(AddressBytesFromString(""));
  EXPECT_FALSE(AddressBytesFromString("FF"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:F"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FF "));
  EXPECT_FALSE(AddressBytesFromString(" FF:FF:FF:FF:FF:FF"));

  auto addr1 = AddressBytesFromString("FF:FF:FF:FF:FF:FF");
  EXPECT_TRUE(addr1);
  EXPECT_EQ("FF:FF:FF:FF:FF:FF", addr1->ToString());

  auto addr2 = AddressBytesFromString("03:7F:FF:02:0F:01");
  EXPECT_TRUE(addr2);
  EXPECT_EQ("03:7F:FF:02:0F:01", addr2->ToString());
}

TEST(FIDL_HelpersTest, AdvertisingIntervalFromFidl) {
  EXPECT_EQ(bt::gap::AdvertisingInterval::FAST1,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::VERY_FAST));
  EXPECT_EQ(bt::gap::AdvertisingInterval::FAST2,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::FAST));
  EXPECT_EQ(bt::gap::AdvertisingInterval::SLOW,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::SLOW));
}

TEST(FIDL_HelpersTest, UuidFromFidl) {
  fbt::Uuid input;
  input.value = {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x0d,
                  0x18, 0x00, 0x00}};

  // We expect the input bytes to be carried over directly.
  bt::UUID output = UuidFromFidl(input);
  EXPECT_EQ("0000180d-0000-1000-8000-00805f9b34fb", output.ToString());
  EXPECT_EQ(2u, output.CompactSize());
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlEmpty) {
  fble::AdvertisingData input;
  ASSERT_TRUE(input.IsEmpty());

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.service_uuids().empty());
  EXPECT_TRUE(output.service_data_uuids().empty());
  EXPECT_TRUE(output.manufacturer_data_ids().empty());
  EXPECT_TRUE(output.uris().empty());
  EXPECT_FALSE(output.appearance());
  EXPECT_FALSE(output.tx_power());
  EXPECT_FALSE(output.local_name());
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlName) {
  constexpr char kTestName[] = "💩";
  fble::AdvertisingData input;
  input.set_name(kTestName);

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.local_name());
  EXPECT_EQ(kTestName, *output.local_name());
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlAppearance) {
  fble::AdvertisingData input;
  input.set_appearance(fuchsia::bluetooth::Appearance::HID_DIGITIZER_TABLET);

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.appearance());

  // Value comes from the standard Bluetooth "assigned numbers" document.
  EXPECT_EQ(0x03C5, *output.appearance());
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlTxPower) {
  constexpr int8_t kTxPower = -50;
  fble::AdvertisingData input;
  input.set_tx_power_level(kTxPower);

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.tx_power());
  EXPECT_EQ(kTxPower, *output.tx_power());
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlUuids) {
  // The first two entries are duplicated. The resulting structure should contain no duplicates.
  const fbt::Uuid kUuid1{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid2{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid3{{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  fble::AdvertisingData input;
  input.set_service_uuids({{kUuid1, kUuid2, kUuid3}});

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_EQ(2u, output.service_uuids().size());
  EXPECT_EQ(1u, output.service_uuids().count(bt::UUID(kUuid1.value)));
  EXPECT_EQ(1u, output.service_uuids().count(bt::UUID(kUuid2.value)));
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlServiceData) {
  const fbt::Uuid kUuid1{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid2{{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  const std::vector<uint8_t> kData1{{'h', 'e', 'l', 'l', 'o'}};
  const std::vector<uint8_t> kData2{{'b', 'y', 'e'}};

  fble::AdvertisingData input;
  input.set_service_data({{{kUuid1, kData1}, {kUuid2, kData2}}});

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_EQ(2u, output.service_data_uuids().size());
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData1), output.service_data(bt::UUID(kUuid1.value))));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData2), output.service_data(bt::UUID(kUuid2.value))));
}

TEST(FIDL_HelpersTest, AdvertisingDataFromFidlManufacturerData) {
  constexpr uint16_t kCompanyId1 = 1;
  constexpr uint16_t kCompanyId2 = 2;
  const std::vector<uint8_t> kData1{{'h', 'e', 'l', 'l', 'o'}};
  const std::vector<uint8_t> kData2{{'b', 'y', 'e'}};

  fble::AdvertisingData input;
  input.set_manufacturer_data({{{kCompanyId1, kData1}, {kCompanyId2, kData2}}});

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_EQ(2u, output.manufacturer_data_ids().size());
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData1), output.manufacturer_data(kCompanyId1)));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData2), output.manufacturer_data(kCompanyId2)));
}

TEST(FIDL_HelpersTest, TechnologyTypeToFidl) {
  EXPECT_EQ(fsys::TechnologyType::LOW_ENERGY,
            TechnologyTypeToFidl(bt::gap::TechnologyType::kLowEnergy));
  EXPECT_EQ(fsys::TechnologyType::CLASSIC, TechnologyTypeToFidl(bt::gap::TechnologyType::kClassic));
  EXPECT_EQ(fsys::TechnologyType::DUAL_MODE,
            TechnologyTypeToFidl(bt::gap::TechnologyType::kDualMode));
}

class FIDL_HelpersAdapterTest : public bthost::testing::AdapterTestFixture {};

TEST_F(FIDL_HelpersAdapterTest, HostInfoToFidl) {
  // Verify that the default parameters are populated as expected.
  auto host_info = HostInfoToFidl(*adapter());
  ASSERT_TRUE(host_info.has_id());
  ASSERT_TRUE(host_info.has_technology());
  ASSERT_TRUE(host_info.has_address());
  ASSERT_TRUE(host_info.has_local_name());
  ASSERT_TRUE(host_info.has_discoverable());
  ASSERT_TRUE(host_info.has_discovering());

  EXPECT_EQ(adapter()->identifier().value(), host_info.id().value);
  EXPECT_EQ(fsys::TechnologyType::DUAL_MODE, host_info.technology());
  EXPECT_EQ(fbt::AddressType::PUBLIC, host_info.address().type);
  EXPECT_TRUE(
      ContainersEqual(adapter()->state().controller_address().bytes(), host_info.address().bytes));
  EXPECT_EQ("fuchsia", host_info.local_name());
  EXPECT_FALSE(host_info.discoverable());
  EXPECT_FALSE(host_info.discovering());
}

TEST(FIDL_HelpersTest, SecurityLevelFromFidl) {
  using FidlSecurityLevel = fuchsia::bluetooth::control::PairingSecurityLevel;
  const FidlSecurityLevel level = FidlSecurityLevel::AUTHENTICATED;
  EXPECT_EQ(bt::sm::SecurityLevel::kAuthenticated, SecurityLevelFromFidl(level));
}

TEST(FIDL_HelpersTest, SecurityLevelFromBadFidlFails) {
  using FidlSecurityLevel = fuchsia::bluetooth::control::PairingSecurityLevel;
  int nonexistant_security_level = 500000;
  auto level = static_cast<FidlSecurityLevel>(nonexistant_security_level);
  EXPECT_EQ(std::nullopt, SecurityLevelFromFidl(level));
}

TEST(FidlHelpersTest, PeerToFidlMandatoryFields) {
  // Required by PeerCache expiry functions.
  async::TestLoop dispatcher;

  bt::gap::PeerCache cache;
  bt::DeviceAddress addr(bt::DeviceAddress::Type::kLEPublic, {0, 1, 2, 3, 4, 5});
  auto* peer = cache.NewPeer(addr, /*connectable=*/true);
  auto fidl = PeerToFidl(*peer);
  ASSERT_TRUE(fidl.has_id());
  EXPECT_EQ(peer->identifier().value(), fidl.id().value);
  ASSERT_TRUE(fidl.has_address());
  EXPECT_TRUE(
      fidl::Equals(fbt::Address{fbt::AddressType::PUBLIC, {{0, 1, 2, 3, 4, 5}}}, fidl.address()));
  ASSERT_TRUE(fidl.has_technology());
  EXPECT_EQ(fsys::TechnologyType::LOW_ENERGY, fidl.technology());
  ASSERT_TRUE(fidl.has_connected());
  EXPECT_FALSE(fidl.connected());
  ASSERT_TRUE(fidl.has_bonded());
  EXPECT_FALSE(fidl.bonded());

  EXPECT_FALSE(fidl.has_name());
  EXPECT_FALSE(fidl.has_appearance());
  EXPECT_FALSE(fidl.has_rssi());
  EXPECT_FALSE(fidl.has_tx_power());
  EXPECT_FALSE(fidl.has_device_class());
  EXPECT_FALSE(fidl.has_services());
}

TEST(FidlHelpersTest, PeerToFidlOptionalFields) {
  // Required by PeerCache expiry functions.
  async::TestLoop dispatcher;

  const int8_t kRssi = 5;
  const int8_t kTxPower = 6;
  const auto kAdv =
      bt::CreateStaticByteBuffer(0x02, 0x01, 0x01,               // Flags: General Discoverable
                                 0x03, 0x19, 192, 0,             // Appearance: Watch
                                 0x02, 0x0A, 0x06,               // Tx-Power: 5
                                 0x05, 0x09, 't', 'e', 's', 't'  // Complete Local Name: "test"
      );

  bt::gap::PeerCache cache;
  bt::DeviceAddress addr(bt::DeviceAddress::Type::kLEPublic, {0, 1, 2, 3, 4, 5});
  auto* peer = cache.NewPeer(addr, /*connectable=*/true);
  peer->MutLe().SetAdvertisingData(kRssi, kAdv);
  peer->MutBrEdr().SetInquiryData(bt::hci::InquiryResult{
      bt::DeviceAddressBytes{{0, 1, 2, 3, 4, 5}}, bt::hci::PageScanRepetitionMode::kR0, 0, 0,
      bt::DeviceClass(bt::DeviceClass::MajorClass::kPeripheral), 0});

  auto fidl = PeerToFidl(*peer);
  ASSERT_TRUE(fidl.has_name());
  EXPECT_EQ("test", fidl.name());
  ASSERT_TRUE(fidl.has_appearance());
  EXPECT_EQ(fbt::Appearance::WATCH, fidl.appearance());
  ASSERT_TRUE(fidl.has_rssi());
  EXPECT_EQ(kRssi, fidl.rssi());
  ASSERT_TRUE(fidl.has_tx_power());
  EXPECT_EQ(kTxPower, fidl.tx_power());
  ASSERT_TRUE(fidl.has_device_class());
  EXPECT_EQ(fbt::MAJOR_DEVICE_CLASS_PERIPHERAL, fidl.device_class().value);

  // TODO(fxb/37485) Add a test when this field gets populated.
  EXPECT_FALSE(fidl.has_services());
}

TEST(FidlHelpersTest, ReliableModeFromFidl) {
  using WriteOptions = fuchsia::bluetooth::gatt::WriteOptions;
  using ReliableMode = fuchsia::bluetooth::gatt::ReliableMode;
  WriteOptions options;

  // No options set, so this should default to disabled.
  EXPECT_EQ(bt::gatt::ReliableMode::kDisabled, ReliableModeFromFidl(options));

  options.set_reliable_mode(ReliableMode::ENABLED);
  EXPECT_EQ(bt::gatt::ReliableMode::kEnabled, ReliableModeFromFidl(options));

  options.set_reliable_mode(ReliableMode::DISABLED);
  EXPECT_EQ(bt::gatt::ReliableMode::kDisabled, ReliableModeFromFidl(options));
}

}  // namespace
}  // namespace fidl_helpers
}  // namespace bthost
