// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

#include <algorithm>
#include <iterator>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/le/cpp/fidl.h"
#include "fuchsia/bluetooth/sys/cpp/fidl.h"
#include "lib/fidl/cpp/comparison.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/sco/sco.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace fble = fuchsia::bluetooth::le;
namespace fbt = fuchsia::bluetooth;
namespace fsys = fuchsia::bluetooth::sys;
namespace fbg = fuchsia::bluetooth::gatt;
namespace fbg2 = fuchsia::bluetooth::gatt2;
namespace fbredr = fuchsia::bluetooth::bredr;
namespace faudio = fuchsia::hardware::audio;

namespace fuchsia::bluetooth {
// Make UUIDs equality comparable for advanced testing matchers. ADL rules mandate the namespace.
bool operator==(const Uuid& a, const Uuid& b) { return fidl::Equals(a, b); }
}  // namespace fuchsia::bluetooth

namespace bthost::fidl_helpers {
namespace {

// Constants as BT stack types
const bt::UInt128 kTestKeyValue{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
const bt::sm::SecurityProperties kTestSecurity(bt::sm::SecurityLevel::kSecureAuthenticated, 16,
                                               /*secure_connections=*/true);
const bt::sm::LTK kTestLtk(kTestSecurity, bt::hci_spec::LinkKey(kTestKeyValue, 0, 0));
const bt::sm::Key kTestKey(kTestSecurity, kTestKeyValue);

// Constants as FIDL types
const fbt::Address kPublicAddrFidl = fbt::Address{fbt::AddressType::PUBLIC, {1, 0, 0, 0, 0, 0}};
const fbt::Address kRandomAddrFidl =
    fbt::Address{fbt::AddressType::RANDOM, {2, 0, 0, 0, 0, 0b11000011}};
const fbt::Address kRandomAddrResolvableFidl{fbt::AddressType::RANDOM,
                                             {0x55, 0x44, 0x33, 0x22, 0x11, 0b01000011}};
const fbt::Address kRandomAddrNonResolvableFidl{fbt::AddressType::RANDOM,
                                                {0x55, 0x44, 0x33, 0x22, 0x11, 0x00}};

const bt::DeviceAddress kTestPeerAddr(bt::DeviceAddress::Type::kBREDR, {1, 0, 0, 0, 0, 0});
const bt::DeviceAddress kLePublicAddress(bt::DeviceAddress::Type::kLEPublic, {1, 0, 0, 0, 0, 0});

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

using HelpersTestWithLoop = ::gtest::TestLoopFixture;

TEST(HelpersTest, HostErrorToFidl) {
  EXPECT_EQ(fsys::Error::FAILED, HostErrorToFidl(bt::HostError::kFailed));
  EXPECT_EQ(fsys::Error::TIMED_OUT, HostErrorToFidl(bt::HostError::kTimedOut));
  EXPECT_EQ(fsys::Error::INVALID_ARGUMENTS, HostErrorToFidl(bt::HostError::kInvalidParameters));
  EXPECT_EQ(fsys::Error::CANCELED, HostErrorToFidl(bt::HostError::kCanceled));
  EXPECT_EQ(fsys::Error::IN_PROGRESS, HostErrorToFidl(bt::HostError::kInProgress));
  EXPECT_EQ(fsys::Error::NOT_SUPPORTED, HostErrorToFidl(bt::HostError::kNotSupported));
  EXPECT_EQ(fsys::Error::PEER_NOT_FOUND, HostErrorToFidl(bt::HostError::kNotFound));

  // All other errors currently map to FAILED.
  EXPECT_EQ(fsys::Error::FAILED, HostErrorToFidl(bt::HostError::kNotReady));
}

TEST(HelpersTest, GattErrorToFidl) {
  // Host errors
  EXPECT_EQ(fbg::Error::INVALID_RESPONSE,
            GattErrorToFidl(bt::Error(bt::HostError::kPacketMalformed)));
  EXPECT_EQ(fbg::Error::FAILURE, GattErrorToFidl(bt::Error(bt::HostError::kTimedOut)));

  // Protocol errors
  EXPECT_EQ(fbg::Error::INSUFFICIENT_AUTHORIZATION,
            GattErrorToFidl(bt::att::Error(bt::att::ErrorCode::kInsufficientAuthorization)));
  EXPECT_EQ(fbg::Error::INSUFFICIENT_AUTHENTICATION,
            GattErrorToFidl(bt::att::Error(bt::att::ErrorCode::kInsufficientAuthentication)));
  EXPECT_EQ(fbg::Error::INSUFFICIENT_ENCRYPTION_KEY_SIZE,
            GattErrorToFidl(bt::att::Error(bt::att::ErrorCode::kInsufficientEncryptionKeySize)));
  EXPECT_EQ(fbg::Error::INSUFFICIENT_ENCRYPTION,
            GattErrorToFidl(bt::att::Error(bt::att::ErrorCode::kInsufficientEncryption)));
  EXPECT_EQ(fbg::Error::READ_NOT_PERMITTED,
            GattErrorToFidl(bt::att::Error(bt::att::ErrorCode::kReadNotPermitted)));
  EXPECT_EQ(fbg::Error::FAILURE,
            GattErrorToFidl(bt::att::Error(bt::att::ErrorCode::kUnlikelyError)));
}

TEST(HelpersTest, AttErrorToGattFidlError) {
  // Host errors
  EXPECT_EQ(fbg2::Error::INVALID_PDU,
            AttErrorToGattFidlError(bt::Error(bt::HostError::kPacketMalformed)));
  EXPECT_EQ(fbg2::Error::INVALID_PARAMETERS,
            AttErrorToGattFidlError(bt::Error(bt::HostError::kInvalidParameters)));
  EXPECT_EQ(fbg2::Error::UNLIKELY_ERROR,
            AttErrorToGattFidlError(bt::Error(bt::HostError::kTimedOut)));

  // Protocol errors
  EXPECT_EQ(
      fbg2::Error::INSUFFICIENT_AUTHORIZATION,
      AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kInsufficientAuthorization)));
  EXPECT_EQ(
      fbg2::Error::INSUFFICIENT_AUTHENTICATION,
      AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kInsufficientAuthentication)));
  EXPECT_EQ(
      fbg2::Error::INSUFFICIENT_ENCRYPTION_KEY_SIZE,
      AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kInsufficientEncryptionKeySize)));
  EXPECT_EQ(fbg2::Error::INSUFFICIENT_ENCRYPTION,
            AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kInsufficientEncryption)));
  EXPECT_EQ(fbg2::Error::READ_NOT_PERMITTED,
            AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kReadNotPermitted)));
  EXPECT_EQ(fbg2::Error::INVALID_HANDLE,
            AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kInvalidHandle)));
  EXPECT_EQ(fbg2::Error::UNLIKELY_ERROR,
            AttErrorToGattFidlError(bt::att::Error(bt::att::ErrorCode::kUnlikelyError)));
}

TEST(HelpersTest, AdvertisingIntervalFromFidl) {
  EXPECT_EQ(bt::gap::AdvertisingInterval::FAST1,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::VERY_FAST));
  EXPECT_EQ(bt::gap::AdvertisingInterval::FAST2,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::FAST));
  EXPECT_EQ(bt::gap::AdvertisingInterval::SLOW,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::SLOW));
}

TEST(HelpersTest, UuidFromFidl) {
  fbt::Uuid input;
  input.value = {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x0d,
                  0x18, 0x00, 0x00}};

  // We expect the input bytes to be carried over directly.
  bt::UUID output = UuidFromFidl(input);
  EXPECT_EQ("0000180d-0000-1000-8000-00805f9b34fb", output.ToString());
  EXPECT_EQ(2u, output.CompactSize());
}

template <typename T>
void FidlToDataElementIntegerTest(const std::function<fbredr::DataElement(T&&)>& func,
                                  bt::sdp::DataElement::Type type) {
  fbredr::DataElement data_element = func(std::numeric_limits<T>::max());
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(type, result->type());
  EXPECT_EQ(std::numeric_limits<T>::max(), result->Get<T>());

  // result->size() returns an enum member of DataElement::Size indicating the number of bytes
  uint8_t exponent = static_cast<uint8_t>(result->size());
  EXPECT_EQ(sizeof(T), std::pow(2, exponent));
}

TEST(HelpersTest, FidlToDataElementInt8Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithInt8),
                               bt::sdp::DataElement::Type::kSignedInt);
}

TEST(HelpersTest, FidlToDataElementInt16Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithInt16),
                               bt::sdp::DataElement::Type::kSignedInt);
}

TEST(HelpersTest, FidlToDataElementInt32Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithInt32),
                               bt::sdp::DataElement::Type::kSignedInt);
}

TEST(HelpersTest, FidlToDataElementInt64Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithInt64),
                               bt::sdp::DataElement::Type::kSignedInt);
}

TEST(HelpersTest, FidlToDataElementUint8Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithUint8),
                               bt::sdp::DataElement::Type::kUnsignedInt);
}

TEST(HelpersTest, FidlToDataElementUint16Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithUint16),
                               bt::sdp::DataElement::Type::kUnsignedInt);
}

TEST(HelpersTest, FidlToDataElementUint32Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithUint32),
                               bt::sdp::DataElement::Type::kUnsignedInt);
}

TEST(HelpersTest, FidlToDataElementUint64Test) {
  FidlToDataElementIntegerTest(std::function(fbredr::DataElement::WithUint64),
                               bt::sdp::DataElement::Type::kUnsignedInt);
}

TEST(HelpersTest, FidlToDataElementEmptyStringTest) {
  std::vector<uint8_t> data;
  ASSERT_EQ(0u, data.size());

  fbredr::DataElement data_element = fbredr::DataElement::WithStr(std::move(data));
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(bt::sdp::DataElement::Type::kString, result->type());
  EXPECT_EQ("", result->Get<std::string>());
  EXPECT_EQ(bt::sdp::DataElement::Size::kNextOne, result->size());
}

TEST(HelpersTest, FidlToDataElementStringTest) {
  std::string expected_str = "foobarbaz";
  std::vector<uint8_t> data(expected_str.size(), 0);
  std::memcpy(data.data(), expected_str.data(), expected_str.size());

  fbredr::DataElement data_element = fbredr::DataElement::WithStr(std::move(data));
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(bt::sdp::DataElement::Type::kString, result->type());
  EXPECT_EQ(expected_str, result->Get<std::string>());
  EXPECT_EQ(bt::sdp::DataElement::Size::kNextOne, result->size());
}

TEST(HelpersTest, FidlToDataElementUrlTest) {
  std::string url = "http://www.google.com";
  std::string moved = url;
  fbredr::DataElement data_element = fbredr::DataElement::WithUrl(std::move(moved));
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(bt::sdp::DataElement::Type::kUrl, result->type());
  EXPECT_EQ(url, result->GetUrl());
  EXPECT_EQ(bt::sdp::DataElement::Size::kNextOne, result->size());
}

TEST(HelpersTest, FidlToDataElementBooleanTest) {
  bool expected = true;
  bool moved = expected;
  fbredr::DataElement data_element = fbredr::DataElement::WithB(std::move(moved));
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(bt::sdp::DataElement::Type::kBoolean, result->type());
  EXPECT_EQ(expected, result->Get<bool>());
  EXPECT_EQ(bt::sdp::DataElement::Size::kOneByte, result->size());
}

TEST(HelpersTest, FidlToDataElementUuidTest) {
  std::unique_ptr<fbt::Uuid> uuid = fbt::Uuid::New();
  uuid->value.fill(123);

  fbredr::DataElement data_element = fbredr::DataElement::WithUuid(std::move(*uuid));
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bt::sdp::DataElement::Type::kUuid, result->type());
  EXPECT_EQ(bt::sdp::DataElement::Size::kSixteenBytes, result->size());

  bt::DynamicByteBuffer bytes(16);
  bytes.Fill(123);

  bt::UUID expected;
  ASSERT_TRUE(bt::UUID::FromBytes(bytes, &expected));
  EXPECT_EQ(expected, result->Get<bt::UUID>());
}

TEST(HelpersTest, FidlToDataElementSequenceTest) {
  int8_t size = 3;
  std::vector<std::unique_ptr<fbredr::DataElement>> moved;
  std::vector<bt::sdp::DataElement> expected;

  for (int16_t i = 0; i < size; i++) {
    expected.emplace_back(i);
    moved.push_back(
        std::make_unique<fbredr::DataElement>(fbredr::DataElement::WithInt16(std::move(i))));
  }

  fbredr::DataElement data_element = fbredr::DataElement::WithSequence(std::move(moved));
  std::optional<bt::sdp::DataElement> result = FidlToDataElement(data_element);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(bt::sdp::DataElement::Type::kSequence, result->type());
  EXPECT_EQ(bt::sdp::DataElement::Size::kNextOne, result->size());

  std::optional<std::vector<bt::sdp::DataElement>> actual =
      result->Get<std::vector<bt::sdp::DataElement>>();
  EXPECT_TRUE(actual);

  for (int8_t i = 0; i < size; i++) {
    EXPECT_EQ(expected[i].Get<int16_t>(), actual.value()[i].Get<int16_t>());
  }
}

TEST(HelpersTest, AdvertisingDataFromFidlEmpty) {
  fble::AdvertisingData input;
  ASSERT_TRUE(input.IsEmpty());

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);

  EXPECT_TRUE(output.service_uuids().empty());
  EXPECT_TRUE(output.service_data_uuids().empty());
  EXPECT_TRUE(output.manufacturer_data_ids().empty());
  EXPECT_TRUE(output.uris().empty());
  EXPECT_FALSE(output.appearance());
  EXPECT_FALSE(output.tx_power());
  EXPECT_FALSE(output.local_name());
}

TEST(HelpersTest, AdvertisingDataFromFidlName) {
  constexpr char kTestName[] = "💩";
  fble::AdvertisingData input;
  input.set_name(kTestName);

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);
  EXPECT_TRUE(output.local_name());
  EXPECT_EQ(kTestName, output.local_name()->name);
}

TEST(HelpersTest, AdvertisingDataFromFidlAppearance) {
  fble::AdvertisingData input;
  input.set_appearance(fuchsia::bluetooth::Appearance::HID_DIGITIZER_TABLET);

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);

  EXPECT_TRUE(output.appearance());

  // Value comes from the standard Bluetooth "assigned numbers" document.
  EXPECT_EQ(0x03C5, *output.appearance());
}

TEST(HelpersTest, AdvertisingDataFromFidlTxPower) {
  constexpr int8_t kTxPower = -50;
  fble::AdvertisingData input;
  input.set_tx_power_level(kTxPower);

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);

  EXPECT_TRUE(output.tx_power());
  EXPECT_EQ(kTxPower, *output.tx_power());
}

TEST(HelpersTest, AdvertisingDataFromFidlUuids) {
  // The first two entries are duplicated. The resulting structure should contain no duplicates.
  const fbt::Uuid kUuid1{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid2{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid3{{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  fble::AdvertisingData input;
  input.set_service_uuids({{kUuid1, kUuid2, kUuid3}});

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);

  EXPECT_EQ(2u, output.service_uuids().size());
  EXPECT_EQ(1u, output.service_uuids().count(bt::UUID(kUuid1.value)));
  EXPECT_EQ(1u, output.service_uuids().count(bt::UUID(kUuid2.value)));
}

TEST(HelpersTest, AdvertisingDataFromFidlServiceData) {
  const fbt::Uuid kUuid1{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid2{{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  const std::vector<uint8_t> kData1{{'h', 'e', 'l', 'l', 'o'}};
  const std::vector<uint8_t> kData2{{'b', 'y', 'e'}};

  fble::AdvertisingData input;
  input.set_service_data({{{kUuid1, kData1}, {kUuid2, kData2}}});

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);
  EXPECT_EQ(2u, output.service_data_uuids().size());
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData1), output.service_data(bt::UUID(kUuid1.value))));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData2), output.service_data(bt::UUID(kUuid2.value))));
}

TEST(HelpersTest, AdvertisingDataFromFidlManufacturerData) {
  constexpr uint16_t kCompanyId1 = 1;
  constexpr uint16_t kCompanyId2 = 2;
  const std::vector<uint8_t> kData1{{'h', 'e', 'l', 'l', 'o'}};
  const std::vector<uint8_t> kData2{{'b', 'y', 'e'}};

  fble::AdvertisingData input;
  input.set_manufacturer_data({{{kCompanyId1, kData1}, {kCompanyId2, kData2}}});

  std::optional<bt::AdvertisingData> maybe_data = AdvertisingDataFromFidl(input);
  ASSERT_TRUE(maybe_data.has_value());
  auto output = std::move(*maybe_data);
  EXPECT_EQ(2u, output.manufacturer_data_ids().size());
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData1), output.manufacturer_data(kCompanyId1)));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData2), output.manufacturer_data(kCompanyId2)));
}

std::string UuidToString(fbt::Uuid uuid) {
  std::string s;
  for (uint8_t byte : uuid.value) {
    s += std::to_string(static_cast<uint16_t>(byte)) + ", ";
  }
  return s;
}
// Each field for this test first attempts to perform the too-long conversion, and then verifies
// that the bounds are where expected by performing a successful conversion with a field that just
// fits in the encoded version. This also enables using the same `input` throughout the test.
TEST(HelpersTest, AdvertisingDataFromFidlWithFieldsTooLong) {
  fble::AdvertisingData input;
  // The length of the AD name field must be <= 248 bytes per v5.2, Vol 4, Part E, 7.3.11 and Vol 3,
  // Part C, 12.1.`
  {
    std::string name_that_fits(bt::kMaxNameLength, 'a');
    std::string too_long_name(bt::kMaxNameLength + 1, 'b');
    input.set_name(too_long_name);
    EXPECT_FALSE(AdvertisingDataFromFidl(input).has_value());
    input.set_name(name_that_fits);
    EXPECT_TRUE(AdvertisingDataFromFidl(input).has_value());
  }
  {
    // This is the longest encoding scheme known to Fuchsia BT, so this represents the longest
    // string allowed (and subsequently, too long to be allowed) by both FIDL and internal
    // invariants.
    std::string uri = "ms-settings-cloudstorage:";
    uri += std::string(fble::MAX_URI_LENGTH - uri.size(), '.');
    input.set_uris({uri});
    EXPECT_FALSE(AdvertisingDataFromFidl(input).has_value());
    // This string should fit when it is one character shorter.
    uri.pop_back();
    input.set_uris({uri});
    EXPECT_TRUE(AdvertisingDataFromFidl(input).has_value());
  }
  // Ensure encoded service data that is too long is rejected.
  {
    const fbt::Uuid kUuid1{.value = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
    // |kUuid1| = 16 bytes, i.e. 14 bytes longer than the shortest possible encoded UUID (2 bytes).
    std::vector too_long_data(fble::MAX_SERVICE_DATA_LENGTH - 13, uint8_t{0xAB});
    input.set_service_data(std::vector({fble::ServiceData{.uuid = kUuid1, .data = too_long_data}}));
    EXPECT_FALSE(AdvertisingDataFromFidl(input).has_value());
    // A vector that is 1 byte shorter than too_long_data should convert successfully
    std::vector data_that_fits(too_long_data.size() - 1, too_long_data[0]);
    input.set_service_data(
        std::vector({fble::ServiceData{.uuid = kUuid1, .data = data_that_fits}}));
    EXPECT_TRUE(AdvertisingDataFromFidl(input).has_value());
  }
  // Ensure encoded manufacturer data that is too long is rejected.
  {
    uint16_t company_id = 0x1212;
    std::vector too_long_data(fble::MAX_MANUFACTURER_DATA_LENGTH + 1, uint8_t{0xAB});
    input.set_manufacturer_data(
        std::vector({fble::ManufacturerData{.company_id = company_id, .data = too_long_data}}));
    EXPECT_FALSE(AdvertisingDataFromFidl(input).has_value());
    // A vector that is 1 byte shorter than too_long_data should convert successfully
    std::vector data_that_fits(too_long_data.size() - 1, too_long_data[0]);
    input.set_manufacturer_data(
        std::vector({fble::ManufacturerData{.company_id = company_id, .data = data_that_fits}}));
    EXPECT_TRUE(AdvertisingDataFromFidl(input).has_value());
  }
  // Ensure input with too many service UUIDs is truncated (NOT rejected).
  {
    std::vector<fbt::Uuid> fbt_uuids;
    fbt::Uuid kBaseUuid{.value = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
    for (int i = 0; i < bt::kMax128BitUuids; ++i) {
      fbt::Uuid next_uuid = kBaseUuid;
      next_uuid.value[0] += i;
      fbt_uuids.push_back(next_uuid);
    }
    input.set_service_uuids(fbt_uuids);
    auto ad = AdvertisingDataFromFidl(input);
    EXPECT_TRUE(ad.has_value());
    std::unordered_set<bt::UUID> converted_uuids = ad->service_uuids();
    for (auto& fbt_uuid : fbt_uuids) {
      SCOPED_TRACE(UuidToString(fbt_uuid));
      EXPECT_TRUE(converted_uuids.find(bt::UUID(fbt_uuid.value)) != converted_uuids.end());
    }
    fbt::Uuid excessive_uuid = kBaseUuid;
    excessive_uuid.value[0] += bt::kMax128BitUuids + 1;
    fbt_uuids.push_back(excessive_uuid);
    input.set_service_uuids(fbt_uuids);
    ad = AdvertisingDataFromFidl(input);
    EXPECT_TRUE(ad.has_value());
    converted_uuids = ad->service_uuids();
    EXPECT_TRUE(converted_uuids.find(bt::UUID(excessive_uuid.value)) == converted_uuids.end());
  }
}

TEST(HelpersTest, AdvertisingDataToFidlDeprecatedEmpty) {
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

TEST(HelpersTest, AdvertisingDataToFidlDeprecated) {
  bt::AdvertisingData input;
  EXPECT_TRUE(input.SetLocalName("fuchsia"));
  input.SetTxPower(4);
  input.SetAppearance(0x1234);

  const uint16_t id = 0x5678;
  const bt::UUID test_uuid = bt::UUID(id);
  bt::StaticByteBuffer service_bytes(0x01, 0x02);
  EXPECT_TRUE(input.AddServiceUuid(test_uuid));
  EXPECT_TRUE(input.SetServiceData(test_uuid, service_bytes.view()));

  uint16_t company_id = 0x98;
  bt::StaticByteBuffer manufacturer_bytes(0x04, 0x03);
  EXPECT_TRUE(input.SetManufacturerData(company_id, manufacturer_bytes.view()));

  auto uri = "http://fuchsia.cl";
  EXPECT_TRUE(input.AddUri(uri));

  auto output = AdvertisingDataToFidlDeprecated(input);

  EXPECT_EQ("fuchsia", output.name);

  auto expected_power_level = std::make_unique<fbt::Int8>();
  expected_power_level->value = 4;
  EXPECT_EQ(expected_power_level->value, output.tx_power_level->value);

  auto expected_appearance = std::make_unique<fbt::UInt16>();
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

TEST(HelpersTest, AdvertisingDataToFidlEmpty) {
  bt::AdvertisingData input;
  auto output = AdvertisingDataToFidl(input);

  // All fields in |input| are not set. Therefore, output should have no set fields as well.
  EXPECT_FALSE(output.has_name());
  EXPECT_FALSE(output.has_tx_power_level());
  EXPECT_FALSE(output.has_appearance());
  EXPECT_FALSE(output.has_service_uuids());
  EXPECT_FALSE(output.has_service_data());
  EXPECT_FALSE(output.has_manufacturer_data());
  EXPECT_FALSE(output.has_uris());
}

TEST(HelpersTest, AdvertisingDataToFidl) {
  bt::AdvertisingData input;
  EXPECT_TRUE(input.SetLocalName("fuchsia"));
  input.SetTxPower(4);
  const uint16_t kAppearance = 193u;  // WATCH_SPORTS
  input.SetAppearance(kAppearance);

  const uint16_t id = 0x5678;
  const bt::UUID test_uuid = bt::UUID(id);
  bt::StaticByteBuffer service_bytes(0x01, 0x02);
  EXPECT_TRUE(input.AddServiceUuid(test_uuid));
  EXPECT_TRUE(input.SetServiceData(test_uuid, service_bytes.view()));

  uint16_t company_id = 0x98;
  bt::StaticByteBuffer manufacturer_bytes(0x04, 0x03);
  EXPECT_TRUE(input.SetManufacturerData(company_id, manufacturer_bytes.view()));

  auto uri = "http://fuchsia.cl/461435";
  EXPECT_TRUE(input.AddUri(uri));

  auto output = AdvertisingDataToFidl(input);

  EXPECT_EQ("fuchsia", output.name());

  auto expected_power_level = std::make_unique<fbt::Int8>();
  expected_power_level->value = 4;
  EXPECT_EQ(expected_power_level->value, output.tx_power_level());

  EXPECT_EQ(fbt::Appearance{kAppearance}, output.appearance());

  EXPECT_EQ(1u, output.service_uuids().size());
  EXPECT_EQ(test_uuid, UuidFromFidl(output.service_uuids().front()));

  EXPECT_EQ(1u, output.service_data().size());
  auto service_data = output.service_data().front();
  EXPECT_EQ(test_uuid, UuidFromFidl(service_data.uuid));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(service_bytes), service_data.data));

  EXPECT_EQ(1u, output.manufacturer_data().size());
  auto manufacturer_data = output.manufacturer_data().front();
  EXPECT_EQ(company_id, manufacturer_data.company_id);
  EXPECT_TRUE(ContainersEqual(bt::BufferView(manufacturer_bytes), manufacturer_data.data));

  EXPECT_THAT(output.uris(), ::testing::ElementsAre(uri));
}

TEST(HelpersTest, AdvertisingDataToFidlOmitsNonEnumeratedAppearance) {
  // There is an "unknown" appearance, which is why this isn't named that.
  const uint16_t kNonEnumeratedAppearance = 0xFFFFu;
  bt::AdvertisingData input;
  input.SetAppearance(kNonEnumeratedAppearance);

  EXPECT_FALSE(AdvertisingDataToFidl(input).has_appearance());

  const uint16_t kKnownAppearance = 832u;  // HEART_RATE_SENSOR
  input.SetAppearance(kKnownAppearance);

  EXPECT_TRUE(AdvertisingDataToFidl(input).has_appearance());
}

TEST(HelpersTest, LeSecurityModeFromFidl) {
  EXPECT_EQ(bt::gap::LESecurityMode::Mode1, LeSecurityModeFromFidl(fsys::LeSecurityMode::MODE_1));
  EXPECT_EQ(bt::gap::LESecurityMode::SecureConnectionsOnly,
            LeSecurityModeFromFidl(fsys::LeSecurityMode::SECURE_CONNECTIONS_ONLY));
  auto nonexistent_security_mode = static_cast<fsys::LeSecurityMode>(0xFF);
  EXPECT_EQ(bt::gap::LESecurityMode::SecureConnectionsOnly,
            LeSecurityModeFromFidl(nonexistent_security_mode));
}

TEST(HelpersTest, TechnologyTypeToFidl) {
  EXPECT_EQ(fsys::TechnologyType::LOW_ENERGY,
            TechnologyTypeToFidl(bt::gap::TechnologyType::kLowEnergy));
  EXPECT_EQ(fsys::TechnologyType::CLASSIC, TechnologyTypeToFidl(bt::gap::TechnologyType::kClassic));
  EXPECT_EQ(fsys::TechnologyType::DUAL_MODE,
            TechnologyTypeToFidl(bt::gap::TechnologyType::kDualMode));
}

TEST(HelpersTest, SecurityLevelFromFidl) {
  const fsys::PairingSecurityLevel level = fsys::PairingSecurityLevel::AUTHENTICATED;
  EXPECT_EQ(bt::sm::SecurityLevel::kAuthenticated, SecurityLevelFromFidl(level));
}

TEST(HelpersTest, SecurityLevelFromBadFidlFails) {
  int nonexistant_security_level = 500000;
  auto level = static_cast<fsys::PairingSecurityLevel>(nonexistant_security_level);
  EXPECT_EQ(std::nullopt, SecurityLevelFromFidl(level));
}

TEST(HelpersTest, PeerToFidlMandatoryFields) {
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
  EXPECT_FALSE(fidl.has_le_services());
  EXPECT_FALSE(fidl.has_bredr_services());
}

TEST(HelpersTest, PeerToFidlOptionalFields) {
  // Required by PeerCache expiry functions.
  async::TestLoop dispatcher;

  const int8_t kRssi = 5;
  const int8_t kTxPower = 6;
  const auto kAdv =
      bt::StaticByteBuffer(0x02, 0x01, 0x01,               // Flags: General Discoverable
                           0x03, 0x19, 192, 0,             // Appearance: Watch
                           0x02, 0x0A, 0x06,               // Tx-Power: 5
                           0x05, 0x09, 't', 'e', 's', 't'  // Complete Local Name: "test"
      );
  const std::vector kBrEdrServices = {bt::UUID(uint16_t{0x110a}), bt::UUID(uint16_t{0x110b})};

  bt::gap::PeerCache cache;
  bt::DeviceAddress addr(bt::DeviceAddress::Type::kLEPublic, {0, 1, 2, 3, 4, 5});
  auto* peer = cache.NewPeer(addr, /*connectable=*/true);
  peer->MutLe().SetAdvertisingData(kRssi, kAdv, zx::time());
  peer->MutBrEdr().SetInquiryData(bt::hci_spec::InquiryResult{
      bt::DeviceAddressBytes{{0, 1, 2, 3, 4, 5}}, bt::hci_spec::PageScanRepetitionMode::kR0, 0, 0,
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

TEST(HelpersTest, ReliableModeFromFidl) {
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
TEST(HelpersTest, ServiceDefinitionToServiceRecord) {
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
  // Add a URL attribute.
  bt::sdp::AttributeId url_attr_id = 0x1112;  // Random ID
  fuchsia::bluetooth::bredr::Attribute url_attribute;
  url_attribute.element.set_url("foobar.dev");
  url_attribute.id = url_attr_id;
  def.mutable_additional_attributes()->emplace_back(std::move(url_attribute));

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
      bt::StaticByteBuffer(0x35, 0x10,  // Data Element Sequence (10 bytes)
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
  auto expected_profile_list = bt::StaticByteBuffer(0x35, 0x08,  // Data Element Sequence (8 bytes)
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
  EXPECT_TRUE(rec.value().HasAttribute(url_attr_id));
  EXPECT_EQ(*rec.value().GetAttribute(url_attr_id).GetUrl(), "foobar.dev");
}

TEST(HelpersTest, FidlToBrEdrSecurityRequirements) {
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

TEST(HelpersTest, AddressFromFidlBondingDataRandomAddressRejectedIfBredr) {
  fsys::BondingData bond;
  bond.set_address(kRandomAddrFidl);
  bond.set_bredr_bond(fsys::BredrBondData());

  EXPECT_EQ(std::nullopt, AddressFromFidlBondingData(bond));
}

TEST(HelpersTest, AddressFromFidlBondingDataBredr) {
  fsys::BondingData bond;
  bond.set_address(kPublicAddrFidl);
  bond.set_bredr_bond(fsys::BredrBondData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kBREDR);
}

TEST(HelpersTest, AddressFromFidlBondingDataDualMode) {
  fsys::BondingData bond;
  bond.set_address(kPublicAddrFidl);
  bond.set_bredr_bond(fsys::BredrBondData());
  bond.set_le_bond(fsys::LeBondData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kBREDR);
}

TEST(HelpersTest, AddressFromFidlBondingDataLePublic) {
  fsys::BondingData bond;
  bond.set_address(kPublicAddrFidl);
  bond.set_le_bond(fsys::LeBondData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kLEPublic);
}

TEST(HelpersTest, AddressFromFidlBondingDataLeRandom) {
  fsys::BondingData bond;
  bond.set_address(kRandomAddrFidl);
  bond.set_le_bond(fsys::LeBondData());

  auto addr = AddressFromFidlBondingData(bond);
  ASSERT_TRUE(addr);
  EXPECT_EQ(addr->type(), bt::DeviceAddress::Type::kLERandom);
}

TEST(HelpersTest, AddressFromFidlBondingDataLeRandomResolvable) {
  fsys::BondingData bond;
  bond.set_address(kRandomAddrResolvableFidl);
  bond.set_le_bond(fsys::LeBondData());

  auto addr = AddressFromFidlBondingData(bond);
  EXPECT_FALSE(addr);
}

TEST(HelpersTest, AddressFromFidlBondingDataLeRandomNonResolvable) {
  fsys::BondingData bond;
  bond.set_address(kRandomAddrNonResolvableFidl);
  bond.set_le_bond(fsys::LeBondData());

  auto addr = AddressFromFidlBondingData(bond);
  EXPECT_FALSE(addr);
}

TEST(HelpersTest, LePairingDataFromFidlEmpty) {
  bt::sm::PairingData result = LePairingDataFromFidl(kLePublicAddress, fsys::LeBondData());
  EXPECT_FALSE(result.identity_address);
  EXPECT_FALSE(result.local_ltk);
  EXPECT_FALSE(result.peer_ltk);
  EXPECT_FALSE(result.irk);
  EXPECT_FALSE(result.csrk);
}

TEST(HelpersTest, LePairingDataFromFidl) {
  fsys::LeBondData le;
  le.set_local_ltk(kTestLtkFidl);
  le.set_peer_ltk(kTestLtkFidl);
  le.set_irk(kTestKeyFidl);
  le.set_csrk(kTestKeyFidl);

  bt::sm::PairingData result = LePairingDataFromFidl(kLePublicAddress, std::move(le));
  ASSERT_TRUE(result.local_ltk);
  ASSERT_TRUE(result.peer_ltk);
  ASSERT_TRUE(result.irk);
  ASSERT_TRUE(result.identity_address);
  ASSERT_TRUE(result.csrk);

  EXPECT_EQ(kTestLtk, *result.local_ltk);
  EXPECT_EQ(kTestLtk, *result.peer_ltk);
  EXPECT_EQ(kTestKey, *result.irk);
  EXPECT_EQ(kLePublicAddress, *result.identity_address);
  EXPECT_EQ(kTestKey, *result.csrk);
}

TEST(HelpersTest, BredrKeyFromFidlEmpty) { EXPECT_FALSE(BredrKeyFromFidl(fsys::BredrBondData())); }

TEST(HelpersTest, BredrKeyFromFidl) {
  const bt::sm::SecurityProperties kTestSecurity(bt::sm::SecurityLevel::kSecureAuthenticated, 16,
                                                 /*secure_connections=*/true);
  const bt::sm::LTK kTestLtk(kTestSecurity, bt::hci_spec::LinkKey(kTestKeyValue, 0, 0));

  fsys::BredrBondData bredr;
  bredr.set_link_key(kTestKeyFidl);
  std::optional<bt::sm::LTK> result = BredrKeyFromFidl(bredr);
  ASSERT_TRUE(result);
  EXPECT_EQ(kTestLtk, *result);
}

TEST(HelpersTest, BredrServicesFromFidlEmpty) {
  EXPECT_TRUE(BredrServicesFromFidl(fsys::BredrBondData()).empty());
}

TEST(HelpersTest, BredrServicesFromFidl) {
  fsys::BredrBondData bredr;
  bredr.mutable_services()->push_back(UuidToFidl(bt::sdp::profile::kAudioSink));
  bredr.mutable_services()->push_back(UuidToFidl(bt::sdp::profile::kAudioSource));
  auto bredr_services = BredrServicesFromFidl(bredr);
  EXPECT_THAT(bredr_services, ::testing::UnorderedElementsAre(bt::sdp::profile::kAudioSource,
                                                              bt::sdp::profile::kAudioSink));
}

class HelpersAdapterTest : public bthost::testing::AdapterTestFixture {};

TEST_F(HelpersAdapterTest, HostInfoToFidl) {
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
      ContainersEqual(adapter()->state().controller_address.bytes(), host_info.address().bytes));
  EXPECT_EQ("fuchsia", host_info.local_name());
  EXPECT_FALSE(host_info.discoverable());
  EXPECT_FALSE(host_info.discovering());
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_NoTransportData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  ASSERT_TRUE(data.has_identifier());
  ASSERT_TRUE(data.has_local_address());
  ASSERT_TRUE(data.has_address());
  EXPECT_FALSE(data.has_name());
  EXPECT_FALSE(data.has_le_bond());
  EXPECT_FALSE(data.has_bredr_bond());

  EXPECT_EQ(peer->identifier().value(), data.identifier().value);
  EXPECT_TRUE(fidl::Equals((fbt::Address{fbt::AddressType::PUBLIC, std::array<uint8_t, 6>{0}}),
                           data.local_address()));
  EXPECT_TRUE(fidl::Equals(kPublicAddrFidl, data.address()));
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_BothTransportsPresentButNotBonded) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutLe();
  peer->MutBrEdr();

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  ASSERT_TRUE(data.has_identifier());
  ASSERT_TRUE(data.has_local_address());
  ASSERT_TRUE(data.has_address());
  EXPECT_FALSE(data.has_le_bond());
  EXPECT_FALSE(data.has_bredr_bond());

  EXPECT_EQ(peer->identifier().value(), data.identifier().value);
  EXPECT_TRUE(fidl::Equals((fbt::Address{fbt::AddressType::PUBLIC, std::array<uint8_t, 6>{0}}),
                           data.local_address()));
  EXPECT_TRUE(fidl::Equals(kPublicAddrFidl, data.address()));
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_BredrServicesDiscoveredNotBonded) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutBrEdr().AddService(bt::UUID(uint16_t{0x1234}));

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_bredr_bond());
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_EmptyLeData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutLe().SetBondData(bt::sm::PairingData());

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_bredr_bond());
  ASSERT_TRUE(data.has_le_bond());
  EXPECT_FALSE(data.le_bond().has_local_ltk());
  EXPECT_FALSE(data.le_bond().has_peer_ltk());
  EXPECT_FALSE(data.le_bond().has_irk());
  EXPECT_FALSE(data.le_bond().has_csrk());
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_LeData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutLe().SetBondData(bt::sm::PairingData{
      .local_ltk = {kTestLtk},
      .peer_ltk = {kTestLtk},
      .irk = {kTestKey},
      .csrk = {kTestKey},
  });

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_bredr_bond());
  ASSERT_TRUE(data.has_le_bond());
  ASSERT_TRUE(data.le_bond().has_local_ltk());
  ASSERT_TRUE(data.le_bond().has_peer_ltk());
  ASSERT_TRUE(data.le_bond().has_irk());
  ASSERT_TRUE(data.le_bond().has_csrk());

  EXPECT_TRUE(fidl::Equals(kTestLtkFidl, data.le_bond().local_ltk()));
  EXPECT_TRUE(fidl::Equals(kTestLtkFidl, data.le_bond().peer_ltk()));
  EXPECT_TRUE(fidl::Equals(kTestKeyFidl, data.le_bond().irk()));
  EXPECT_TRUE(fidl::Equals(kTestKeyFidl, data.le_bond().csrk()));
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_BredrData) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutBrEdr().SetBondData(kTestLtk);

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  EXPECT_FALSE(data.has_le_bond());
  ASSERT_TRUE(data.has_bredr_bond());
  ASSERT_TRUE(data.bredr_bond().has_link_key());
  EXPECT_TRUE(fidl::Equals(kTestKeyFidl, data.bredr_bond().link_key()));
}

TEST_F(HelpersAdapterTest, PeerToFidlBondingData_IncludesBredrServices) {
  auto* peer = adapter()->peer_cache()->NewPeer(kTestPeerAddr, /*connectable=*/true);
  peer->MutBrEdr().SetBondData(kTestLtk);
  peer->MutBrEdr().AddService(bt::sdp::profile::kAudioSink);
  peer->MutBrEdr().AddService(bt::sdp::profile::kAudioSource);

  fsys::BondingData data = PeerToFidlBondingData(*adapter(), *peer);
  ASSERT_TRUE(data.has_bredr_bond());
  ASSERT_TRUE(data.bredr_bond().has_services());

  EXPECT_THAT(data.bredr_bond().services(),
              ::testing::UnorderedElementsAre(UuidToFidl(bt::sdp::profile::kAudioSink),
                                              UuidToFidl(bt::sdp::profile::kAudioSource)));
}

// Returns a copy to avoid forming references to misaligned fields, which is UB.
template <typename T>
T copy(T t) {
  return t;
}

TEST_F(HelpersAdapterTest, FidlToScoParameters) {
  fbredr::ScoConnectionParameters params;
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_parameter_set(fbredr::HfpParameterSet::MSBC_T2);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_air_coding_format(fbredr::CodingFormat::MSBC);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_air_frame_size(8u);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_io_bandwidth(32000);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_io_coding_format(fbredr::CodingFormat::LINEAR_PCM);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_io_frame_size(16u);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_io_pcm_data_format(faudio::SampleFormat::PCM_SIGNED);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_io_pcm_sample_payload_msb_position(3u);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());
  params.set_path(fbredr::DataPath::OFFLOAD);
  ASSERT_TRUE(FidlToScoParameters(params).is_ok());

  bt::EmbossStruct<bt::hci_spec::SynchronousConnectionParametersWriter> out =
      FidlToScoParameters(params).take_value();
  auto view = out.view();
  EXPECT_EQ(view.transmit_bandwidth().Read(), 8000u);
  EXPECT_EQ(view.receive_bandwidth().Read(), 8000u);

  EXPECT_EQ(view.transmit_coding_format().coding_format().Read(), bt::hci_spec::CodingFormat::MSBC);
  EXPECT_EQ(view.transmit_coding_format().company_id().Read(), 0u);
  EXPECT_EQ(view.transmit_coding_format().vendor_codec_id().Read(), 0u);

  EXPECT_EQ(view.receive_coding_format().coding_format().Read(), bt::hci_spec::CodingFormat::MSBC);
  EXPECT_EQ(view.receive_coding_format().company_id().Read(), 0u);
  EXPECT_EQ(view.receive_coding_format().vendor_codec_id().Read(), 0u);

  EXPECT_EQ(view.transmit_codec_frame_size_bytes().Read(), 8u);
  EXPECT_EQ(view.receive_codec_frame_size_bytes().Read(), 8u);

  EXPECT_EQ(view.input_bandwidth().Read(), 32000u);
  EXPECT_EQ(view.output_bandwidth().Read(), 32000u);

  EXPECT_EQ(view.input_coding_format().coding_format().Read(),
            bt::hci_spec::CodingFormat::LINEAR_PCM);
  EXPECT_EQ(view.input_coding_format().company_id().Read(), 0u);
  EXPECT_EQ(view.input_coding_format().vendor_codec_id().Read(), 0u);

  EXPECT_EQ(view.output_coding_format().coding_format().Read(),
            bt::hci_spec::CodingFormat::LINEAR_PCM);
  EXPECT_EQ(view.output_coding_format().company_id().Read(), 0u);
  EXPECT_EQ(view.output_coding_format().vendor_codec_id().Read(), 0u);

  EXPECT_EQ(view.input_coded_data_size_bits().Read(), 16u);
  EXPECT_EQ(view.output_coded_data_size_bits().Read(), 16u);

  EXPECT_EQ(view.input_pcm_data_format().Read(), bt::hci_spec::PcmDataFormat::TWOS_COMPLEMENT);
  EXPECT_EQ(view.output_pcm_data_format().Read(), bt::hci_spec::PcmDataFormat::TWOS_COMPLEMENT);

  EXPECT_EQ(view.input_pcm_sample_payload_msb_position().Read(), 3u);
  EXPECT_EQ(view.output_pcm_sample_payload_msb_position().Read(), 3u);

  EXPECT_EQ(view.input_data_path().Read(), static_cast<bt::hci_spec::ScoDataPath>(6));
  EXPECT_EQ(view.output_data_path().Read(), static_cast<bt::hci_spec::ScoDataPath>(6));

  EXPECT_EQ(view.input_transport_unit_size_bits().Read(), 0u);
  EXPECT_EQ(view.output_transport_unit_size_bits().Read(), 0u);

  EXPECT_EQ(view.max_latency_ms().Read(), bt::sco::kParameterSetMsbcT2.max_latency_ms);
  EXPECT_EQ(view.packet_types().BackingStorage().ReadUInt(),
            bt::sco::kParameterSetMsbcT2.packet_types);
  EXPECT_EQ(view.retransmission_effort().Read(),
            static_cast<bt::hci_spec::SynchronousConnectionParameters::ScoRetransmissionEffort>(
                bt::sco::kParameterSetMsbcT2.retransmission_effort));

  // When the IO coding format is Linear PCM, the PCM data format is required.
  params.clear_io_pcm_data_format();
  EXPECT_TRUE(FidlToScoParameters(params).is_error());

  // PCM_FLOAT is not a supported PCM format.
  params.set_io_pcm_data_format(faudio::SampleFormat::PCM_FLOAT);
  EXPECT_TRUE(FidlToScoParameters(params).is_error());

  // PCM format for non-PCM IO coding formats is kNotApplicable and MSB is 0.
  params.set_io_coding_format(fbredr::CodingFormat::TRANSPARENT);
  ASSERT_TRUE(FidlToScoParameters(params).is_ok());
  out = FidlToScoParameters(params).value();
  EXPECT_EQ(view.input_pcm_data_format().Read(), bt::hci_spec::PcmDataFormat::NOT_APPLICABLE);
  EXPECT_EQ(view.input_pcm_sample_payload_msb_position().Read(), 0u);
}

TEST(HelpersTest, DiscoveryFilterFromEmptyFidlFilter) {
  bt::gap::DiscoveryFilter filter = DiscoveryFilterFromFidl(fble::Filter());
  EXPECT_TRUE(filter.service_uuids().empty());
  EXPECT_TRUE(filter.service_data_uuids().empty());
  EXPECT_FALSE(filter.manufacturer_code().has_value());
  EXPECT_FALSE(filter.connectable().has_value());
  EXPECT_TRUE(filter.name_substring().empty());
  EXPECT_FALSE(filter.pathloss().has_value());
}

TEST(HelpersTest, DiscoveryFilterFromFidlFilter) {
  fble::Filter fidl_filter;
  fuchsia::bluetooth::Uuid service_uuid;
  service_uuid.value[0] = 1;
  fuchsia::bluetooth::Uuid service_data_uuid;
  service_uuid.value[0] = 2;
  fidl_filter.set_service_uuid(service_uuid);
  fidl_filter.set_service_data_uuid(service_data_uuid);
  fidl_filter.set_manufacturer_id(2);
  fidl_filter.set_connectable(true);
  fidl_filter.set_name("name");
  fidl_filter.set_max_path_loss(3);
  bt::gap::DiscoveryFilter filter = DiscoveryFilterFromFidl(fidl_filter);
  EXPECT_THAT(filter.service_uuids(), ::testing::ElementsAre(service_uuid.value));
  EXPECT_THAT(filter.service_data_uuids(), ::testing::ElementsAre(service_data_uuid.value));
  ASSERT_TRUE(filter.manufacturer_code().has_value());
  EXPECT_EQ(filter.manufacturer_code().value(), 2u);
  ASSERT_TRUE(filter.connectable().has_value());
  EXPECT_EQ(filter.connectable().value(), true);
  EXPECT_EQ(filter.name_substring(), "name");
  ASSERT_TRUE(filter.pathloss().has_value());
  ASSERT_EQ(filter.pathloss().value(), 3);
}

TEST(HelpersTest, EmptyAdvertisingDataToFidlScanData) {
  bt::AdvertisingData input;
  fble::ScanData output = AdvertisingDataToFidlScanData(input, zx::time(1));
  EXPECT_FALSE(output.has_tx_power());
  EXPECT_FALSE(output.has_appearance());
  EXPECT_FALSE(output.has_service_uuids());
  EXPECT_FALSE(output.has_service_data());
  EXPECT_FALSE(output.has_manufacturer_data());
  EXPECT_FALSE(output.has_uris());
  ASSERT_TRUE(output.has_timestamp());
  EXPECT_EQ(output.timestamp(), zx::time(1).get());
}

TEST(HelpersTest, AdvertisingDataToFidlScanData) {
  bt::AdvertisingData input;
  input.SetTxPower(4);
  const uint16_t kAppearance = 193u;  // WATCH_SPORTS
  input.SetAppearance(kAppearance);

  const uint16_t id = 0x5678;
  const bt::UUID kServiceUuid = bt::UUID(id);
  auto service_bytes = bt::StaticByteBuffer(0x01, 0x02);
  EXPECT_TRUE(input.AddServiceUuid(kServiceUuid));
  EXPECT_TRUE(input.SetServiceData(kServiceUuid, service_bytes.view()));

  const uint16_t kManufacturer = 0x98;
  auto manufacturer_bytes = bt::StaticByteBuffer(0x04, 0x03);
  EXPECT_TRUE(input.SetManufacturerData(kManufacturer, manufacturer_bytes.view()));

  const char* const kUri = "http://fuchsia.cl/461435";
  EXPECT_TRUE(input.AddUri(kUri));

  fble::ScanData output = AdvertisingDataToFidlScanData(input, zx::time(1));
  EXPECT_EQ(4, output.tx_power());
  EXPECT_EQ(fbt::Appearance{kAppearance}, output.appearance());
  ASSERT_EQ(1u, output.service_uuids().size());
  EXPECT_EQ(kServiceUuid, UuidFromFidl(output.service_uuids().front()));
  ASSERT_EQ(1u, output.service_data().size());
  ASSERT_TRUE(output.has_timestamp());
  EXPECT_EQ(output.timestamp(), zx::time(1).get());
  auto service_data = output.service_data().front();
  EXPECT_EQ(kServiceUuid, UuidFromFidl(service_data.uuid));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(service_bytes), service_data.data));
  EXPECT_EQ(1u, output.manufacturer_data().size());
  auto manufacturer_data = output.manufacturer_data().front();
  EXPECT_EQ(kManufacturer, manufacturer_data.company_id);
  EXPECT_TRUE(ContainersEqual(bt::BufferView(manufacturer_bytes), manufacturer_data.data));
  EXPECT_THAT(output.uris(), ::testing::ElementsAre(kUri));
}

TEST(HelpersTest, AdvertisingDataToFidlScanDataOmitsNonEnumeratedAppearance) {
  // There is an "unknown" appearance, which is why this isn't named that.
  const uint16_t kNonEnumeratedAppearance = 0xFFFFu;
  bt::AdvertisingData input;
  input.SetAppearance(kNonEnumeratedAppearance);

  EXPECT_FALSE(AdvertisingDataToFidlScanData(input, zx::time()).has_appearance());

  const uint16_t kKnownAppearance = 832u;  // HEART_RATE_SENSOR
  input.SetAppearance(kKnownAppearance);

  EXPECT_TRUE(AdvertisingDataToFidlScanData(input, zx::time()).has_appearance());
}

TEST_F(HelpersTestWithLoop, PeerToFidlLeWithAllFields) {
  RunLoopFor(zx::duration(2));

  const bt::PeerId kPeerId(1);
  const bt::DeviceAddress kAddr(bt::DeviceAddress::Type::kLEPublic, {1, 0, 0, 0, 0, 0});
  bt::gap::PeerMetrics metrics;
  bt::gap::Peer peer([](auto&, auto) {}, [](auto&) {}, [](auto&) {}, [](auto&) { return false; },
                     kPeerId, kAddr,
                     /*connectable=*/true, &metrics);
  peer.RegisterName("name");
  const int8_t kRssi = 1;
  auto adv_bytes = bt::StaticByteBuffer(
      // Uri: "https://abc.xyz"
      0x0B, bt::DataType::kURI, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z');
  peer.MutLe().SetAdvertisingData(kRssi, adv_bytes, zx::time(1));

  fble::Peer fidl_peer = PeerToFidlLe(peer);
  ASSERT_TRUE(fidl_peer.has_id());
  EXPECT_EQ(fidl_peer.id().value, kPeerId.value());
  ASSERT_TRUE(fidl_peer.has_bonded());
  EXPECT_FALSE(fidl_peer.bonded());
  ASSERT_TRUE(fidl_peer.has_name());
  EXPECT_EQ(fidl_peer.name(), "name");
  ASSERT_TRUE(fidl_peer.has_rssi());
  EXPECT_EQ(fidl_peer.rssi(), kRssi);
  ASSERT_TRUE(fidl_peer.has_data());
  EXPECT_THAT(fidl_peer.data().uris(), ::testing::ElementsAre("https://abc.xyz"));
  ASSERT_TRUE(fidl_peer.has_last_updated());
  EXPECT_EQ(fidl_peer.last_updated(), 2);
  ASSERT_TRUE(fidl_peer.data().has_timestamp());
  EXPECT_EQ(fidl_peer.data().timestamp(), 1);
}

TEST_F(HelpersTestWithLoop, PeerToFidlLeWithoutAdvertisingData) {
  const bt::PeerId kPeerId(1);
  const bt::DeviceAddress kAddr(bt::DeviceAddress::Type::kLEPublic, {1, 0, 0, 0, 0, 0});
  bt::gap::PeerMetrics metrics;
  bt::gap::Peer peer([](auto&, auto) {}, [](auto&) {}, [](auto&) {}, [](auto&) { return false; },
                     kPeerId, kAddr,
                     /*connectable=*/true, &metrics);

  fble::Peer fidl_peer = PeerToFidlLe(peer);
  ASSERT_TRUE(fidl_peer.has_id());
  EXPECT_EQ(fidl_peer.id().value, kPeerId.value());
  ASSERT_TRUE(fidl_peer.has_bonded());
  EXPECT_FALSE(fidl_peer.bonded());
  EXPECT_FALSE(fidl_peer.has_name());
  EXPECT_FALSE(fidl_peer.has_rssi());
  EXPECT_FALSE(fidl_peer.has_data());
  ASSERT_TRUE(fidl_peer.has_last_updated());
  EXPECT_EQ(fidl_peer.last_updated(), 0);
}

TEST(HelpersTest, PeerIdFromString) {
  EXPECT_FALSE(PeerIdFromString(std::string()));
  EXPECT_FALSE(PeerIdFromString("-1"));
  EXPECT_FALSE(PeerIdFromString("g"));
  EXPECT_EQ(PeerIdFromString("0"), std::optional(bt::PeerId(0)));
  EXPECT_EQ(PeerIdFromString("FFFFFFFFFFFFFFFF"),
            std::optional(bt::PeerId(std::numeric_limits<uint64_t>::max())));
  // ID is 1 more than max.
  EXPECT_FALSE(PeerIdFromString("10000000000000000"));
  EXPECT_EQ(PeerIdFromString("0123456789"), std::optional(bt::PeerId(0x123456789)));
  EXPECT_EQ(PeerIdFromString("abcdef"), std::optional(bt::PeerId(0xabcdef)));
  EXPECT_EQ(PeerIdFromString("ABCDEF"), std::optional(bt::PeerId(0xabcdef)));
}

TEST(HelpersTest, ScoPacketStatusToFidl) {
  EXPECT_EQ(
      ScoPacketStatusToFidl(bt::hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived),
      fuchsia::bluetooth::bredr::RxPacketStatus::CORRECTLY_RECEIVED_DATA);
  EXPECT_EQ(ScoPacketStatusToFidl(bt::hci_spec::SynchronousDataPacketStatusFlag::kPossiblyInvalid),
            fuchsia::bluetooth::bredr::RxPacketStatus::POSSIBLY_INVALID_DATA);
  EXPECT_EQ(ScoPacketStatusToFidl(bt::hci_spec::SynchronousDataPacketStatusFlag::kNoDataReceived),
            fuchsia::bluetooth::bredr::RxPacketStatus::NO_DATA_RECEIVED);
  EXPECT_EQ(
      ScoPacketStatusToFidl(bt::hci_spec::SynchronousDataPacketStatusFlag::kDataPartiallyLost),
      fuchsia::bluetooth::bredr::RxPacketStatus::DATA_PARTIALLY_LOST);
}

TEST(HelpersTest, Gatt2DescriptorFromFidlNoPermissions) {
  fbg2::Descriptor desc;
  fbt::Uuid fidl_uuid{{}};
  desc.set_type(fidl_uuid);
  desc.set_handle(fbg2::Handle{3});
  EXPECT_EQ(Gatt2DescriptorFromFidl(desc), nullptr);
}

TEST(HelpersTest, Gatt2DescriptorFromFidlNoType) {
  fbg2::Descriptor desc;
  fbg2::AttributePermissions permissions;
  desc.set_permissions(std::move(permissions));
  desc.set_handle(fbg2::Handle{3});
  EXPECT_EQ(Gatt2DescriptorFromFidl(desc), nullptr);
}

TEST(HelpersTest, Gatt2DescriptorFromFidlNoHandle) {
  fbg2::Descriptor desc;
  fbg2::AttributePermissions permissions;
  desc.set_permissions(std::move(permissions));
  fbt::Uuid fidl_uuid{{}};
  desc.set_type(fidl_uuid);
  EXPECT_EQ(Gatt2DescriptorFromFidl(desc), nullptr);
}

TEST(HelpersTest, Gatt2DescriptorFromFidlSuccessWithEmptyPermissions) {
  bt::UInt128 type = {2};

  fbg2::Descriptor desc;
  fbg2::AttributePermissions permissions;
  desc.set_permissions(std::move(permissions));
  fbt::Uuid fidl_uuid{type};
  desc.set_type(fidl_uuid);
  desc.set_handle(fbg2::Handle{3});

  std::unique_ptr<bt::gatt::Descriptor> out = Gatt2DescriptorFromFidl(desc);
  ASSERT_TRUE(out);
  EXPECT_EQ(out->id(), 3u);
  EXPECT_EQ(out->type(), bt::UUID(type));
  EXPECT_FALSE(out->read_permissions().allowed());
  EXPECT_FALSE(out->write_permissions().allowed());
}

TEST(HelpersTest, Gatt2DescriptorFromFidlSuccessWithEmptyReadWritePermissions) {
  fbg2::Descriptor desc;
  fbg2::AttributePermissions permissions;
  permissions.set_read(fbg2::SecurityRequirements());
  permissions.set_write(fbg2::SecurityRequirements());
  desc.set_permissions(std::move(permissions));
  fbt::Uuid fidl_uuid{{}};
  desc.set_type(fidl_uuid);
  desc.set_handle(fbg2::Handle{3});

  std::unique_ptr<bt::gatt::Descriptor> out = Gatt2DescriptorFromFidl(desc);
  ASSERT_TRUE(out);
  EXPECT_TRUE(out->read_permissions().allowed_without_security());
  EXPECT_TRUE(out->write_permissions().allowed_without_security());
}

TEST(HelpersTest, Gatt2DescriptorFromFidlSuccessWithReadPermissions) {
  fbg2::Descriptor desc;
  fbg2::AttributePermissions permissions;
  fbg2::SecurityRequirements read_reqs;
  read_reqs.set_authentication_required(true);
  read_reqs.set_authorization_required(true);
  read_reqs.set_encryption_required(true);
  permissions.set_read(std::move(read_reqs));
  desc.set_permissions(std::move(permissions));
  desc.set_type({{}});
  desc.set_handle(fbg2::Handle{3});

  std::unique_ptr<bt::gatt::Descriptor> out = Gatt2DescriptorFromFidl(desc);
  ASSERT_TRUE(out);
  EXPECT_TRUE(out->read_permissions().encryption_required());
  EXPECT_TRUE(out->read_permissions().authentication_required());
  EXPECT_TRUE(out->read_permissions().authorization_required());
  EXPECT_FALSE(out->write_permissions().allowed());
}

TEST(HelpersTest, Gatt2DescriptorFromFidlSuccessWithWritePermissions) {
  fbg2::Descriptor desc;
  fbg2::AttributePermissions permissions;
  fbg2::SecurityRequirements write_reqs;
  write_reqs.set_authentication_required(true);
  write_reqs.set_authorization_required(true);
  write_reqs.set_encryption_required(true);
  permissions.set_write(std::move(write_reqs));
  desc.set_permissions(std::move(permissions));
  desc.set_type({{}});
  desc.set_handle(fbg2::Handle{3});

  std::unique_ptr<bt::gatt::Descriptor> out = Gatt2DescriptorFromFidl(desc);
  ASSERT_TRUE(out);
  EXPECT_FALSE(out->read_permissions().allowed());
  EXPECT_TRUE(out->write_permissions().encryption_required());
  EXPECT_TRUE(out->write_permissions().authentication_required());
  EXPECT_TRUE(out->write_permissions().authorization_required());
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlNoProperties) {
  fbg2::Characteristic chrc;
  chrc.set_permissions(fbg2::AttributePermissions());
  chrc.set_type(fbt::Uuid{{}});
  chrc.set_handle(fbg2::Handle{3});
  EXPECT_FALSE(Gatt2CharacteristicFromFidl(chrc));
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlNoPermissions) {
  fbg2::Characteristic chrc;
  chrc.mutable_properties();
  chrc.set_type(fbt::Uuid{{}});
  chrc.set_handle(fbg2::Handle{3});
  EXPECT_FALSE(Gatt2CharacteristicFromFidl(chrc));
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlSuccessWithPropertiesAndEmptyPermissions) {
  fbg2::Characteristic chrc;
  chrc.set_properties(fbg2::CharacteristicPropertyBits::WRITE |
                      fbg2::CharacteristicPropertyBits::RELIABLE_WRITE);
  chrc.set_permissions(fbg2::AttributePermissions());
  bt::UInt128 type = {2};
  chrc.set_type(fbt::Uuid{type});
  chrc.set_handle(fbg2::Handle{3});
  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_TRUE(out);
  EXPECT_EQ(out->properties(),
            bt::gatt::Property::kWrite | bt::gatt::Property::kExtendedProperties);
  EXPECT_EQ(out->extended_properties(), bt::gatt::ExtendedProperty::kReliableWrite);
  EXPECT_FALSE(out->write_permissions().allowed());
  EXPECT_FALSE(out->read_permissions().allowed());
  EXPECT_FALSE(out->update_permissions().allowed());
  EXPECT_EQ(out->type(), bt::UUID(type));
  EXPECT_EQ(out->id(), 3u);
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlSupportsNotifyButDoesNotHavePermission) {
  fbg2::Characteristic chrc;
  chrc.set_properties(fbg2::CharacteristicPropertyBits::NOTIFY);
  chrc.set_permissions(fbg2::AttributePermissions());
  bt::UInt128 type = {2};
  chrc.set_type(fbt::Uuid{type});
  chrc.set_handle(fbg2::Handle{3});
  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_FALSE(out);
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlDoesNotSupportNotifyButDoesHaveUpdatePermission) {
  fbg2::Characteristic chrc;
  chrc.mutable_properties();
  fbg2::AttributePermissions permissions;
  permissions.set_update(fbg2::SecurityRequirements());
  chrc.set_permissions(std::move(permissions));
  bt::UInt128 type = {2};
  chrc.set_type(fbt::Uuid{type});
  chrc.set_handle(fbg2::Handle{3});
  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_FALSE(out);
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlSuccessWithEmptySecurityRequirements) {
  fbg2::Characteristic chrc;
  chrc.set_properties(fbg2::CharacteristicPropertyBits::NOTIFY);
  fbg2::AttributePermissions permissions;
  permissions.set_read(fbg2::SecurityRequirements());
  permissions.set_write(fbg2::SecurityRequirements());
  permissions.set_update(fbg2::SecurityRequirements());
  chrc.set_permissions(std::move(permissions));
  bt::UInt128 type = {2};
  chrc.set_type(fbt::Uuid{type});
  chrc.set_handle(fbg2::Handle{3});
  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_TRUE(out);
  EXPECT_TRUE(out->write_permissions().allowed_without_security());
  EXPECT_TRUE(out->read_permissions().allowed_without_security());
  EXPECT_TRUE(out->update_permissions().allowed_without_security());
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlSuccessWithAllSecurityRequirements) {
  fbg2::Characteristic chrc;
  chrc.set_properties(fbg2::CharacteristicPropertyBits::NOTIFY);
  fbg2::AttributePermissions permissions;
  fbg2::SecurityRequirements read_reqs;
  read_reqs.set_authentication_required(true);
  read_reqs.set_authorization_required(true);
  read_reqs.set_encryption_required(true);
  permissions.set_read(std::move(read_reqs));
  fbg2::SecurityRequirements write_reqs;
  write_reqs.set_authentication_required(true);
  write_reqs.set_authorization_required(true);
  write_reqs.set_encryption_required(true);
  permissions.set_write(std::move(write_reqs));
  fbg2::SecurityRequirements update_reqs;
  update_reqs.set_authentication_required(true);
  update_reqs.set_authorization_required(true);
  update_reqs.set_encryption_required(true);
  permissions.set_update(std::move(update_reqs));
  chrc.set_permissions(std::move(permissions));
  bt::UInt128 type = {2};
  chrc.set_type(fbt::Uuid{type});
  chrc.set_handle(fbg2::Handle{3});
  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_TRUE(out);
  EXPECT_TRUE(out->write_permissions().authentication_required());
  EXPECT_TRUE(out->write_permissions().authorization_required());
  EXPECT_TRUE(out->write_permissions().encryption_required());
  EXPECT_TRUE(out->read_permissions().authentication_required());
  EXPECT_TRUE(out->read_permissions().authorization_required());
  EXPECT_TRUE(out->read_permissions().encryption_required());
  EXPECT_TRUE(out->update_permissions().authentication_required());
  EXPECT_TRUE(out->update_permissions().authorization_required());
  EXPECT_TRUE(out->update_permissions().encryption_required());
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlWithInvalidDescriptor) {
  fbg2::Characteristic chrc;
  chrc.mutable_properties();
  chrc.set_permissions(fbg2::AttributePermissions());
  bt::UInt128 type = {2};
  chrc.set_type(fbt::Uuid{type});
  chrc.set_handle(fbg2::Handle{3});
  std::vector<fbg2::Descriptor> descriptors;
  descriptors.emplace_back(fbg2::Descriptor());
  chrc.set_descriptors(std::move(descriptors));
  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_FALSE(out);
}

TEST(HelpersTest, Gatt2CharacteristicFromFidlWith2Descriptors) {
  fbg2::Characteristic chrc;
  chrc.mutable_properties();
  chrc.set_permissions(fbg2::AttributePermissions());
  bt::UInt128 chrc_type = {2};
  chrc.set_type(fbt::Uuid{chrc_type});
  chrc.set_handle(fbg2::Handle{3});

  std::vector<fbg2::Descriptor> descriptors;

  fbg2::Descriptor desc_0;
  desc_0.set_permissions(fbg2::AttributePermissions());
  bt::UInt128 desc_type_0 = {4};
  desc_0.set_type(fbt::Uuid{desc_type_0});
  desc_0.set_handle(fbg2::Handle{5});
  descriptors.push_back(std::move(desc_0));

  fbg2::Descriptor desc_1;
  desc_1.set_permissions(fbg2::AttributePermissions());
  bt::UInt128 desc_type_1 = {6};
  desc_1.set_type(fbt::Uuid{desc_type_1});
  desc_1.set_handle(fbg2::Handle{7});
  descriptors.push_back(std::move(desc_1));

  chrc.set_descriptors(std::move(descriptors));

  std::unique_ptr<bt::gatt::Characteristic> out = Gatt2CharacteristicFromFidl(chrc);
  ASSERT_TRUE(out);
  ASSERT_EQ(out->descriptors().size(), 2u);
  EXPECT_EQ(out->descriptors()[0]->id(), 5u);
  EXPECT_EQ(out->descriptors()[0]->type(), bt::UUID(desc_type_0));
  EXPECT_FALSE(out->descriptors()[0]->read_permissions().allowed());
  EXPECT_FALSE(out->descriptors()[0]->write_permissions().allowed());
  EXPECT_EQ(out->descriptors()[1]->id(), 7u);
  EXPECT_EQ(out->descriptors()[1]->type(), bt::UUID(desc_type_1));
  EXPECT_FALSE(out->descriptors()[1]->read_permissions().allowed());
  EXPECT_FALSE(out->descriptors()[1]->write_permissions().allowed());
}

}  // namespace
}  // namespace bthost::fidl_helpers
