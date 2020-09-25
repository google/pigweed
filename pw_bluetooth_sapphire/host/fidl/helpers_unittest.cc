// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

#include <algorithm>
#include <iterator>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "adapter_test_fixture.h"
#include "fuchsia/bluetooth/control/cpp/fidl.h"
#include "fuchsia/bluetooth/sys/cpp/fidl.h"
#include "lib/fidl/cpp/comparison.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace fble = fuchsia::bluetooth::le;
namespace fbt = fuchsia::bluetooth;
namespace fsys = fuchsia::bluetooth::sys;
namespace fbg = fuchsia::bluetooth::gatt;
namespace fbredr = fuchsia::bluetooth::bredr;

namespace fuchsia::bluetooth {
// Make UUIDs equality comparable for advanced testing matchers. ADL rules mandate the namespace.
bool operator==(const Uuid& a, const Uuid& b) { return fidl::Equals(a, b); }
}  // namespace fuchsia::bluetooth

namespace bthost {
namespace fidl_helpers {
namespace {

// Constants as BT stack types
const bt::UInt128 kTestKeyValue{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
const bt::sm::SecurityProperties kTestSecurity(bt::sm::SecurityLevel::kSecureAuthenticated, 16,
                                               true);
const bt::sm::LTK kTestLtk(kTestSecurity, bt::hci::LinkKey(kTestKeyValue, 0, 0));
const bt::sm::Key kTestKey(kTestSecurity, kTestKeyValue);

// Constants as FIDL types
const fbt::Address kPublicAddrFidl = fbt::Address{fbt::AddressType::PUBLIC, {1, 0, 0, 0, 0, 0}};
const fbt::Address kRandomAddrFidl = fbt::Address{fbt::AddressType::RANDOM, {2, 0, 0, 0, 0, 0}};

const bt::DeviceAddress kTestPeerAddr(bt::DeviceAddress::Type::kBREDR, {1, 0, 0, 0, 0, 0});

const fsys::PeerKey kTestKeyFidl{
    .security =
        fsys::SecurityProperties{
            .authenticated = true,
            .secure_connections = true,
            .encryption_key_size = 16,
        },
    .data = fsys::Key{.value = kTestKeyValue},
};
const fsys::Ltk kTestLtkFidl{.key = kTestKeyFidl, .ediv = 0, .rand = 0};

TEST(FIDL_HelpersTest, HostErrorToFidl) {
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

TEST(FIDL_HelpersTest, GattStatusToFidl) {
  // Host errors
  EXPECT_EQ(fbg::Error::INVALID_RESPONSE,
            GattStatusToFidl(bt::att::Status(bt::HostError::kPacketMalformed)));
  EXPECT_EQ(fbg::Error::FAILURE, GattStatusToFidl(bt::att::Status(bt::HostError::kTimedOut)));

  // Protocol errors
  EXPECT_EQ(fbg::Error::INSUFFICIENT_AUTHORIZATION,
            GattStatusToFidl(bt::att::Status(bt::att::ErrorCode::kInsufficientAuthorization)));
  EXPECT_EQ(fbg::Error::INSUFFICIENT_AUTHENTICATION,
            GattStatusToFidl(bt::att::Status(bt::att::ErrorCode::kInsufficientAuthentication)));
  EXPECT_EQ(fbg::Error::INSUFFICIENT_ENCRYPTION_KEY_SIZE,
            GattStatusToFidl(bt::att::Status(bt::att::ErrorCode::kInsufficientEncryptionKeySize)));
  EXPECT_EQ(fbg::Error::INSUFFICIENT_ENCRYPTION,
            GattStatusToFidl(bt::att::Status(bt::att::ErrorCode::kInsufficientEncryption)));
  EXPECT_EQ(fbg::Error::READ_NOT_PERMITTED,
            GattStatusToFidl(bt::att::Status(bt::att::ErrorCode::kReadNotPermitted)));
  EXPECT_EQ(fbg::Error::FAILURE,
            GattStatusToFidl(bt::att::Status(bt::att::ErrorCode::kUnlikelyError)));
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

TEST(FIDL_HelpersTest, AdvertisingDataToFidlDeprecatedEmpty) {
  bt::AdvertisingData input;
  auto output = AdvertisingDataToFidlDeprecated(input);

  // All fields in |input| are not set. Therefore, output should have no set fields as well.
  EXPECT_FALSE(output.name);
  EXPECT_FALSE(output.tx_power_level);
  EXPECT_FALSE(output.appearance);
  EXPECT_FALSE(output.service_uuids);
  EXPECT_FALSE(output.service_data);
  EXPECT_FALSE(output.manufacturer_specific_data);
  EXPECT_FALSE(output.solicited_service_uuids);
  EXPECT_FALSE(output.uris);
}

TEST(FIDL_HelpersTest, AdvertisingDataToFidlDeprecated) {
  bt::AdvertisingData input;
  input.SetLocalName("fuchsia");
  input.SetTxPower(4);
  input.SetAppearance(0x1234);

  const uint16_t id = 0x5678;
  const bt::UUID test_uuid = bt::UUID(id);
  auto service_bytes = bt::CreateStaticByteBuffer(0x01, 0x02);
  input.AddServiceUuid(test_uuid);
  input.SetServiceData(test_uuid, service_bytes.view());

  uint16_t company_id = 0x98;
  auto manufacturer_bytes = bt::CreateStaticByteBuffer(0x04, 0x03);
  input.SetManufacturerData(company_id, manufacturer_bytes.view());

  auto uri = "http://fuchsia.cl";
  input.AddURI(uri);

  auto output = AdvertisingDataToFidlDeprecated(input);

  EXPECT_EQ("fuchsia", output.name);

  auto expected_power_level = fbt::Int8::New();
  expected_power_level->value = 4;
  EXPECT_EQ(expected_power_level->value, output.tx_power_level->value);

  auto expected_appearance = fbt::UInt16::New();
  expected_appearance->value = 0x1234;
  EXPECT_EQ(expected_appearance->value, output.appearance->value);

  EXPECT_EQ(1u, output.service_uuids->size());
  EXPECT_EQ(test_uuid.ToString(), output.service_uuids->front());

  EXPECT_EQ(1u, output.service_data->size());
  auto service_data = output.service_data->front();
  EXPECT_EQ(test_uuid.ToString(), service_data.uuid);
  EXPECT_TRUE(ContainersEqual(bt::BufferView(service_bytes), service_data.data));

  EXPECT_EQ(1u, output.manufacturer_specific_data->size());
  auto manufacturer_data = output.manufacturer_specific_data->front();
  EXPECT_EQ(company_id, manufacturer_data.company_id);
  EXPECT_TRUE(ContainersEqual(bt::BufferView(manufacturer_bytes), manufacturer_data.data));

  EXPECT_EQ(1u, output.uris->size());
  EXPECT_EQ(uri, output.uris->front());
}

TEST(FIDL_HelpersTest, LeSecurityModeFromFidl) {
  EXPECT_EQ(bt::gap::LeSecurityMode::Mode1, LeSecurityModeFromFidl(fsys::LeSecurityMode::MODE_1));
  EXPECT_EQ(bt::gap::LeSecurityMode::SecureConnectionsOnly,
            LeSecurityModeFromFidl(fsys::LeSecurityMode::SECURE_CONNECTIONS_ONLY));
  auto nonexistent_security_mode = static_cast<fsys::LeSecurityMode>(0xFF);
  EXPECT_EQ(bt::gap::LeSecurityMode::SecureConnectionsOnly,
            LeSecurityModeFromFidl(nonexistent_security_mode));
}

TEST(FIDL_HelpersTest, TechnologyTypeToFidl) {
  EXPECT_EQ(fsys::TechnologyType::LOW_ENERGY,
            TechnologyTypeToFidl(bt::gap::TechnologyType::kLowEnergy));
  EXPECT_EQ(fsys::TechnologyType::CLASSIC, TechnologyTypeToFidl(bt::gap::TechnologyType::kClassic));
  EXPECT_EQ(fsys::TechnologyType::DUAL_MODE,
            TechnologyTypeToFidl(bt::gap::TechnologyType::kDualMode));
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

TEST(FIDL_HelpersTest, PeerToFidlMandatoryFields) {
  // Required by PeerCache expiry functions.
  async::TestLoop dispatcher;

  inspect::Inspector inspector;
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
  EXPECT_FALSE(fidl.has_le_services());
  EXPECT_FALSE(fidl.has_bredr_services());
}

TEST(FIDL_HelpersTest, PeerToFidlOptionalFields) {
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
  const std::vector kBrEdrServices = {bt::UUID(uint16_t{0x110a}), bt::UUID(uint16_t{0x110b})};

  inspect::Inspector inspector;
  bt::gap::PeerCache cache;
  bt::DeviceAddress addr(bt::DeviceAddress::Type::kLEPublic, {0, 1, 2, 3, 4, 5});
  auto* peer = cache.NewPeer(addr, /*connectable=*/true);
  peer->MutLe().SetAdvertisingData(kRssi, kAdv);
  peer->MutBrEdr().SetInquiryData(bt::hci::InquiryResult{
      bt::DeviceAddressBytes{{0, 1, 2, 3, 4, 5}}, bt::hci::PageScanRepetitionMode::kR0, 0, 0,
      bt::DeviceClass(bt::DeviceClass::MajorClass::kPeripheral), 0});
  for (auto& service : kBrEdrServices) {
    peer->MutBrEdr().AddService(service);
  }

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

  // Deprecated and never implemented (see fxbug.dev/57344).
  EXPECT_FALSE(fidl.has_services());

  // TODO(fxbug.dev/57344): Add a test when this field gets populated.
  EXPECT_FALSE(fidl.has_le_services());

  ASSERT_TRUE(fidl.has_bredr_services());
  std::vector<fbt::Uuid> expected_uuids;
  std::transform(kBrEdrServices.begin(), kBrEdrServices.end(), std::back_inserter(expected_uuids),
                 UuidToFidl);
  EXPECT_THAT(fidl.bredr_services(), ::testing::UnorderedElementsAreArray(expected_uuids));
}

TEST(FIDL_HelpersTest, ReliableModeFromFidl) {
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

// TODO: Set information w/o setting language, set a FIDL type that cannot be converted
// - make sure the expected attributes are set and have the correct type
// - make sure the profile descriptor sets the right attributes
TEST(FIDL_HelpersTest, ServiceDefinitionToServiceRecord) {
  fuchsia::bluetooth::bredr::ServiceDefinition def_should_fail;
  // Should fail to convert without service class UUIDs.
  auto rec_no_uuids = ServiceDefinitionToServiceRecord(def_should_fail);
  EXPECT_FALSE(rec_no_uuids.is_ok());
  // Should fail to convert when information set without language.
  def_should_fail.mutable_service_class_uuids()->emplace_back(
      fidl_helpers::UuidToFidl(bt::sdp::profile::kAudioSink));
  fuchsia::bluetooth::bredr::Information info_no_language;
  def_should_fail.mutable_information()->emplace_back(std::move(info_no_language));
  auto rec_no_language = ServiceDefinitionToServiceRecord(def_should_fail);
  EXPECT_FALSE(rec_no_language.is_ok());

  // Create definition for successful conversion.
  fuchsia::bluetooth::bredr::ServiceDefinition def;
  def.mutable_service_class_uuids()->emplace_back(
      fidl_helpers::UuidToFidl(bt::sdp::profile::kAudioSink));
  fuchsia::bluetooth::bredr::Information info;
  info.set_language("en");
  info.set_name("TEST");
  def.mutable_information()->emplace_back(std::move(info));
  fuchsia::bluetooth::bredr::ProtocolDescriptor l2cap_proto;
  l2cap_proto.protocol = fuchsia::bluetooth::bredr::ProtocolIdentifier::L2CAP;
  fuchsia::bluetooth::bredr::DataElement l2cap_data_el;
  l2cap_data_el.set_uint16(fuchsia::bluetooth::bredr::PSM_SDP);
  l2cap_proto.params.emplace_back(std::move(l2cap_data_el));
  def.mutable_protocol_descriptor_list()->emplace_back(std::move(l2cap_proto));
  fuchsia::bluetooth::bredr::ProtocolDescriptor avdtp_proto;
  avdtp_proto.protocol = fuchsia::bluetooth::bredr::ProtocolIdentifier::AVDTP;
  fuchsia::bluetooth::bredr::DataElement avdtp_data_el;
  avdtp_data_el.set_uint16(0x0103);  // Version 1.3
  avdtp_proto.params.emplace_back(std::move(avdtp_data_el));
  def.mutable_protocol_descriptor_list()->emplace_back(std::move(avdtp_proto));
  fuchsia::bluetooth::bredr::ProfileDescriptor prof_desc;
  prof_desc.profile_id =
      fuchsia::bluetooth::bredr::ServiceClassProfileIdentifier::ADVANCED_AUDIO_DISTRIBUTION;
  prof_desc.major_version = 1;
  prof_desc.minor_version = 3;
  def.mutable_profile_descriptors()->emplace_back(prof_desc);
  bt::sdp::AttributeId valid_att_id = 0x1111;
  fuchsia::bluetooth::bredr::Attribute valid_attribute;
  valid_attribute.element.set_uint8(0x01);
  valid_attribute.id = valid_att_id;
  def.mutable_additional_attributes()->emplace_back(std::move(valid_attribute));
  // Add an invalid additional attribute that should not convert.
  bt::sdp::AttributeId invalid_att_id = 0x1112;
  fuchsia::bluetooth::bredr::Attribute invalid_attribute;
  invalid_attribute.element.set_url("");
  invalid_attribute.id = invalid_att_id;
  def.mutable_additional_attributes()->emplace_back(std::move(invalid_attribute));

  // Confirm converted ServiceRecord fields match ServiceDefinition
  auto rec = ServiceDefinitionToServiceRecord(def);
  EXPECT_TRUE(rec.is_ok());

  // Confirm UUIDs match
  std::unordered_set<bt::UUID> attribute_uuid = {bt::sdp::profile::kAudioSink};
  EXPECT_TRUE(rec.value().FindUUID(attribute_uuid));

  // Confirm information fields match
  EXPECT_TRUE(rec.value().HasAttribute(bt::sdp::kLanguageBaseAttributeIdList));
  const bt::sdp::DataElement& lang_val =
      rec.value().GetAttribute(bt::sdp::kLanguageBaseAttributeIdList);
  auto triplets = lang_val.Get<std::vector<bt::sdp::DataElement>>();
  EXPECT_TRUE(triplets);
  EXPECT_TRUE(triplets->size() % 3 == 0);
  EXPECT_EQ(bt::sdp::DataElement::Type::kUnsignedInt, triplets->at(0).type());
  EXPECT_EQ(bt::sdp::DataElement::Type::kUnsignedInt, triplets->at(1).type());
  EXPECT_EQ(bt::sdp::DataElement::Type::kUnsignedInt, triplets->at(2).type());
  auto lang = triplets->at(0).Get<uint16_t>();
  EXPECT_TRUE(lang);
  EXPECT_EQ(0x656e, *lang);  // should be 'en' in ascii (but big-endian)
  auto encoding = triplets->at(1).Get<uint16_t>();
  EXPECT_TRUE(encoding);
  EXPECT_EQ(106, *encoding);  // should always be UTF-8
  auto base_attrid = triplets->at(2).Get<uint16_t>();
  EXPECT_TRUE(base_attrid);
  EXPECT_EQ(0x0100, *base_attrid);  // The primary language must be at 0x0100.
  EXPECT_TRUE(rec.value().HasAttribute(*base_attrid + bt::sdp::kServiceNameOffset));
  const bt::sdp::DataElement& name_elem =
      rec.value().GetAttribute(*base_attrid + bt::sdp::kServiceNameOffset);
  auto name = name_elem.Get<std::string>();
  EXPECT_TRUE(name);
  EXPECT_EQ("TEST", *name);

  // Confirm protocol +  descriptor list
  EXPECT_TRUE(rec.value().HasAttribute(bt::sdp::kProtocolDescriptorList));
  const bt::sdp::DataElement& protocol_val =
      rec.value().GetAttribute(bt::sdp::kProtocolDescriptorList);
  bt::DynamicByteBuffer protocol_block(protocol_val.WriteSize());
  protocol_val.Write(&protocol_block);
  auto expected_protocol_list =
      bt::CreateStaticByteBuffer(0x35, 0x10,  // Data Element Sequence (10 bytes)
                                 0x35, 0x06,  // Data Element Sequence (6 bytes)
                                 0x19,        // UUID (16 bits)
                                 0x01, 0x00,  // L2CAP Profile UUID
                                 0x09,        // uint16_t
                                 0x00, 0x01,  // PSM = SDP
                                 0x35, 0x06,  // Data Element Sequence (6 bytes)
                                 0x19,        // UUID
                                 0x00, 0x19,  // AVTDP Profile UUID
                                 0x09,        // uint16_t
                                 0x01, 0x03   // PSM_AVDTP
      );
  EXPECT_EQ(expected_protocol_list.size(), protocol_block.size());
  EXPECT_TRUE(ContainersEqual(expected_protocol_list, protocol_block));

  // Confirm profile descriptor list
  EXPECT_TRUE(rec.value().HasAttribute(bt::sdp::kBluetoothProfileDescriptorList));
  const bt::sdp::DataElement& profile_val =
      rec.value().GetAttribute(bt::sdp::kBluetoothProfileDescriptorList);
  bt::DynamicByteBuffer profile_block(profile_val.WriteSize());
  profile_val.Write(&profile_block);
  auto expected_profile_list =
      bt::CreateStaticByteBuffer(0x35, 0x08,  // Data Element Sequence (8 bytes)
                                 0x35, 0x06,  // Data Element Sequence (6 bytes)
                                 0x19,        // UUID
                                 0x11, 0x0d,  // Advanced Audio Identifier
                                 0x09,        // uint16_t
                                 0x01, 0x03   // Major and minor version
      );
  EXPECT_EQ(expected_profile_list.size(), profile_block.size());
  EXPECT_TRUE(ContainersEqual(expected_profile_list, profile_block));

  // Confirm additional attributes
  EXPECT_TRUE(rec.value().HasAttribute(valid_att_id));
  EXPECT_FALSE(rec.value().HasAttribute(invalid_att_id));
}

TEST(FIDL_HelpersTest, FidlToBrEdrSecurityRequirements) {
  fbredr::ChannelParameters params;
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));

  params.set_security_requirements(fbredr::SecurityRequirements());
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));

  params.mutable_security_requirements()->set_secure_connections_required(false);
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));
  params.mutable_security_requirements()->clear_secure_connections_required();

  params.mutable_security_requirements()->set_authentication_required(false);
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));

  params.mutable_security_requirements()->set_secure_connections_required(false);
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));

  params.mutable_security_requirements()->set_authentication_required(true);
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = true, .secure_connections = false}));

  params.mutable_security_requirements()->set_secure_connections_required(true);
  EXPECT_EQ(
      FidlToBrEdrSecurityRequirements(params),
      (bt::gap::BrEdrSecurityRequirements{.authentication = true, .secure_connections = true}));
}

TEST(FIDL_HelpersTest, AddressFromFidlBondingDataRandomAddressRejectedIfBredr) {
  fsys::BondingData bond;
  bond.set_address(kRandomAddrFidl);
  bond.set_bredr(fsys::BredrData());

  EXPECT_EQ(std::nullopt, AddressFromFidlBondingData(bond));
}

TEST(FIDL_HelpersTest, AddressFromFidlBondingDataBredr) {
  fsys::BondingData bond;
  bond.set_address(kPublicAddrFidl);
  bond.set_bredr(fsys::BredrData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kBREDR);
}

TEST(FIDL_HelpersTest, AddressFromFidlBondingDataDualMode) {
  fsys::BondingData bond;
  bond.set_address(kPublicAddrFidl);
  bond.set_bredr(fsys::BredrData());
  bond.set_le(fsys::LeData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kBREDR);
}

TEST(FIDL_HelpersTest, AddressFromFidlBondingDataLePublic) {
  fsys::BondingData bond;
  bond.set_address(kPublicAddrFidl);
  bond.set_le(fsys::LeData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kLEPublic);
}

TEST(FIDL_HelpersTest, AddressFromFidlBondingDataLeRandom) {
  fsys::BondingData bond;
  bond.set_address(kRandomAddrFidl);
  bond.set_le(fsys::LeData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kLERandom);
}

TEST(FIDL_HelpersTest, LePairingDataFromFidlEmpty) {
  bt::sm::PairingData result = LePairingDataFromFidl(fsys::LeData());
  EXPECT_FALSE(result.identity_address);
  EXPECT_FALSE(result.local_ltk);
  EXPECT_FALSE(result.peer_ltk);
  EXPECT_FALSE(result.irk);
  EXPECT_FALSE(result.csrk);
}

TEST(FIDL_HelpersTest, LePairingDataFromFidl) {
  fsys::LeData le;
  le.set_local_ltk(kTestLtkFidl);
  le.set_peer_ltk(kTestLtkFidl);
  le.set_irk(kTestKeyFidl);
  le.set_csrk(kTestKeyFidl);

  bt::sm::PairingData result = LePairingDataFromFidl(std::move(le));
  EXPECT_FALSE(result.identity_address);
  ASSERT_TRUE(result.local_ltk);
  ASSERT_TRUE(result.peer_ltk);
  ASSERT_TRUE(result.irk);
  ASSERT_TRUE(result.csrk);

  EXPECT_EQ(kTestLtk, *result.local_ltk);
  EXPECT_EQ(kTestLtk, *result.peer_ltk);
  EXPECT_EQ(kTestKey, *result.irk);
  EXPECT_EQ(kTestKey, *result.csrk);
}

TEST(FIDL_HelpersTest, BredrKeyFromFidlEmpty) { EXPECT_FALSE(BredrKeyFromFidl(fsys::BredrData())); }

TEST(FIDL_HelpersTest, BredrKeyFromFidl) {
  const bt::sm::SecurityProperties kTestSecurity(bt::sm::SecurityLevel::kSecureAuthenticated, 16,
                                                 true);
  const bt::sm::LTK kTestLtk(kTestSecurity, bt::hci::LinkKey(kTestKeyValue, 0, 0));

  fsys::BredrData bredr;
  bredr.set_link_key(kTestKeyFidl);
  std::optional<bt::sm::LTK> result = BredrKeyFromFidl(bredr);
  ASSERT_TRUE(result);
  EXPECT_EQ(kTestLtk, *result);
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

TEST_F(FIDL_HelpersAdapterTest, PeerToFidlBondingData_NoTransportData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  ASSERT_TRUE(data.has_identifier());
  ASSERT_TRUE(data.has_local_address());
  ASSERT_TRUE(data.has_address());
  EXPECT_FALSE(data.has_name());
  EXPECT_FALSE(data.has_le());
  EXPECT_FALSE(data.has_bredr());

  EXPECT_EQ(peer->identifier().value(), data.identifier().value);
  EXPECT_TRUE(fidl::Equals((fbt::Address{fbt::AddressType::PUBLIC, std::array<uint8_t, 6>{0}}),
                           data.local_address()));
  EXPECT_TRUE(fidl::Equals(kPublicAddrFidl, data.address()));
}

TEST_F(FIDL_HelpersAdapterTest, PeerToFidlBondingData_BothTransportsPresentButNotBonded) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutLe();
  peer->MutBrEdr();

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  ASSERT_TRUE(data.has_identifier());
  ASSERT_TRUE(data.has_local_address());
  ASSERT_TRUE(data.has_address());
  EXPECT_FALSE(data.has_le());
  EXPECT_FALSE(data.has_bredr());

  EXPECT_EQ(peer->identifier().value(), data.identifier().value);
  EXPECT_TRUE(fidl::Equals((fbt::Address{fbt::AddressType::PUBLIC, std::array<uint8_t, 6>{0}}),
                           data.local_address()));
  EXPECT_TRUE(fidl::Equals(kPublicAddrFidl, data.address()));
}

TEST_F(FIDL_HelpersAdapterTest, PeerToFidlBondingData_EmptyLeData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutLe().SetBondData(bt::sm::PairingData());

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_bredr());
  ASSERT_TRUE(data.has_le());
  EXPECT_FALSE(data.le().has_local_ltk());
  EXPECT_FALSE(data.le().has_peer_ltk());
  EXPECT_FALSE(data.le().has_irk());
  EXPECT_FALSE(data.le().has_csrk());
}

TEST_F(FIDL_HelpersAdapterTest, PeerToFidlBondingData_LeData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutLe().SetBondData(bt::sm::PairingData{
      .local_ltk = {kTestLtk},
      .peer_ltk = {kTestLtk},
      .irk = {kTestKey},
      .csrk = {kTestKey},
  });

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_bredr());
  ASSERT_TRUE(data.has_le());
  ASSERT_TRUE(data.le().has_local_ltk());
  ASSERT_TRUE(data.le().has_peer_ltk());
  ASSERT_TRUE(data.le().has_irk());
  ASSERT_TRUE(data.le().has_csrk());

  EXPECT_TRUE(fidl::Equals(kTestLtkFidl, data.le().local_ltk()));
  EXPECT_TRUE(fidl::Equals(kTestLtkFidl, data.le().peer_ltk()));
  EXPECT_TRUE(fidl::Equals(kTestKeyFidl, data.le().irk()));
  EXPECT_TRUE(fidl::Equals(kTestKeyFidl, data.le().csrk()));
}

TEST_F(FIDL_HelpersAdapterTest, PeerToFidlBondingData_BredrData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutBrEdr().SetBondData(kTestLtk);

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_le());
  ASSERT_TRUE(data.has_bredr());
  ASSERT_TRUE(data.bredr().has_link_key());
  EXPECT_TRUE(fidl::Equals(kTestKeyFidl, data.bredr().link_key()));
}

}  // namespace
}  // namespace fidl_helpers
}  // namespace bthost
