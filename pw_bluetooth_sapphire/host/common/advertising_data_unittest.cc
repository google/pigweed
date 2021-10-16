// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"

#include <limits>
#include <variant>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt {
namespace {

constexpr uint16_t kGattUuid = 0x1801;
constexpr uint16_t kEddystoneUuid = 0xFEAA;
constexpr uint16_t kHeartRateServiceUuid = 0x180D;

constexpr uint16_t kId1As16 = 0x0212;
constexpr uint16_t kId2As16 = 0x1122;

constexpr size_t kRandomDataSize = 100;

TEST(AdvertisingDataTest, MakeEmpty) {
  AdvertisingData data;

  EXPECT_EQ(0u, data.CalculateBlockSize());
}

TEST(AdvertisingDataTest, EncodeKnownURI) {
  AdvertisingData data;
  EXPECT_TRUE(data.AddUri("https://abc.xyz"));

  auto bytes =
      CreateStaticByteBuffer(0x0B, 0x24, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z');

  EXPECT_EQ(bytes.size(), data.CalculateBlockSize());
  DynamicByteBuffer block(data.CalculateBlockSize());
  data.WriteBlock(&block, std::nullopt);
  EXPECT_TRUE(ContainersEqual(bytes, block));
}

TEST(AdvertisingDataTest, EncodeUnknownURI) {
  AdvertisingData data;
  EXPECT_TRUE(data.AddUri("flubs:xyz"));

  auto bytes =
      CreateStaticByteBuffer(0x0B, 0x24, 0x01, 'f', 'l', 'u', 'b', 's', ':', 'x', 'y', 'z');

  size_t block_size = data.CalculateBlockSize();
  EXPECT_EQ(bytes.size(), block_size);
  DynamicByteBuffer block(block_size);
  data.WriteBlock(&block, std::nullopt);
  EXPECT_TRUE(ContainersEqual(bytes, block));
}

TEST(AdvertisingDataTest, CompressServiceUUIDs) {
  AdvertisingData data;
  std::unordered_set<UUID> uuids{UUID(kId1As16), UUID(kId2As16)};
  for (auto& uuid : uuids) {
    SCOPED_TRACE(bt_str(uuid));
    EXPECT_TRUE(data.AddServiceUuid(uuid));
  }

  uint8_t expected_block_size = 1                          // length byte
                                + 1                        // type byte
                                + (sizeof(uint16_t) * 2);  // 2 16-bit UUIDs
  EXPECT_EQ(expected_block_size, data.CalculateBlockSize());

  StaticByteBuffer expected_header{expected_block_size - 1, DataType::kIncomplete16BitServiceUuids};

  DynamicByteBuffer block(expected_block_size);
  data.WriteBlock(&block, std::nullopt);

  EXPECT_TRUE(ContainersEqual(expected_header, block.view(/*pos=*/0, /*size=*/2)));
  auto to_uuid = [](const ByteBuffer& b, size_t pos) {
    return UUID(b.view(pos, /*size=*/2).To<uint16_t>());
  };
  EXPECT_TRUE(uuids.find(to_uuid(block, 2)) != uuids.end());
  EXPECT_TRUE(uuids.find(to_uuid(block, 4)) != uuids.end());
}

TEST(AdvertisingDataTest, ParseBlock) {
  auto bytes = CreateStaticByteBuffer(
      // Complete 16-bit UUIDs
      0x05, 0x03, 0x12, 0x02, 0x22, 0x11,
      // Incomplete list of 32-bit UUIDs
      0x05, 0x04, 0x34, 0x12, 0x34, 0x12,
      // Local name
      0x09, 0x09, 'T', 'e', 's', 't', 0xF0, 0x9F, 0x92, 0x96,
      // TX Power
      0x02, 0x0A, 0x8F);

  AdvertisingData::ParseResult data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.is_ok());

  EXPECT_EQ(3u, data->service_uuids().size());
  EXPECT_TRUE(data->local_name());
  EXPECT_EQ("Test💖", *(data->local_name()));
  EXPECT_TRUE(data->tx_power());
  EXPECT_EQ(-113, *(data->tx_power()));
}

TEST(AdvertisingDataTest, ParseBlockUnknownDataType) {
  AdvertisingData expected_ad;
  constexpr uint8_t lower_byte = 0x12, upper_byte = 0x22;
  constexpr uint16_t uuid_value = (upper_byte << 8) + lower_byte;
  // The only field present in the expected AD is one complete 16-bit UUID.
  EXPECT_TRUE(expected_ad.AddServiceUuid(UUID(uuid_value)));

  auto bytes = StaticByteBuffer{
      // Complete 16-bit UUIDs
      0x03, 0x03, lower_byte, upper_byte,
      // 0x40, the second octet, is not a recognized DataType (see common/supplement_data.h).
      0x05, 0x40, 0x34, 0x12, 0x34, 0x12};
  AdvertisingData::ParseResult data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.is_ok());

  // The second field of `bytes` was valid (in that its length byte matched its length), but its
  // Data Type was unknown, so it should be ignored (i.e. the only field in the `data` should be the
  // single 16-bit UUID, matching expected AD).
  EXPECT_EQ(expected_ad, *data);
}

TEST(AdvertisingDataTest, ParseBlockNameTooLong) {
  // A block with a name of exactly kMaxNameLength (==248) bytes should be parsed correctly.
  {
    auto leading_bytes = StaticByteBuffer<2>{kMaxNameLength + 1, DataType::kCompleteLocalName};
    auto bytes = DynamicByteBuffer(kMaxNameLength + 2);
    bytes.Write(leading_bytes);
    DynamicByteBuffer name(kMaxNameLength);
    name.Fill('a');
    bytes.Write(name, /*pos=*/2);
    AdvertisingData::ParseResult result = AdvertisingData::FromBytes(bytes);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->local_name(), std::string(kMaxNameLength, 'a'));
  }
  // A block with a name of kMaxNameLength+1 (==249) bytes should be rejected.
  {
    auto leading_bytes = StaticByteBuffer<2>{kMaxNameLength + 2, DataType::kCompleteLocalName};
    auto bytes = DynamicByteBuffer(kMaxNameLength + 3);
    bytes.Write(leading_bytes);
    DynamicByteBuffer name(kMaxNameLength + 1);
    name.Fill('a');
    bytes.Write(name, /*pos=*/2);
    AdvertisingData::ParseResult result = AdvertisingData::FromBytes(bytes);
    ASSERT_TRUE(result.is_error());
    EXPECT_EQ(AdvertisingData::ParseError::kLocalNameTooLong, result.error_value());
  }
}

TEST(AdvertisingDataTest, ManufacturerZeroLength) {
  auto bytes = CreateStaticByteBuffer(
      // Complete 16-bit UUIDs
      0x05, 0x03, 0x12, 0x02, 0x22, 0x11,
      // Manufacturer Data with no data
      0x03, 0xFF, 0x34, 0x12);

  EXPECT_EQ(0u, AdvertisingData().manufacturer_data_ids().size());

  AdvertisingData::ParseResult data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.is_ok());

  EXPECT_EQ(1u, data->manufacturer_data_ids().count(0x1234));
  EXPECT_EQ(0u, data->manufacturer_data(0x1234).size());
}

TEST(AdvertisingDataTest, ServiceData) {
  // A typical Eddystone-URL beacon advertisement
  // to "https://fuchsia.cl"
  auto bytes = CreateStaticByteBuffer(
      // Complete 16-bit UUIDs, 0xFEAA
      0x03, 0x03, 0xAA, 0xFE,
      // Eddystone Service (0xFEAA) Data:
      0x10, 0x16, 0xAA, 0xFE,
      0x10,  // Eddystone-Uri type
      0xEE,  // TX Power level -18dBm
      0x03,  // "https://"
      'f', 'u', 'c', 'h', 's', 'i', 'a', '.', 'c', 'l');

  EXPECT_EQ(0u, AdvertisingData().service_data_uuids().size());

  AdvertisingData::ParseResult data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.is_ok());

  UUID eddystone(uint16_t{0xFEAA});

  EXPECT_EQ(1u, data->service_data_uuids().size());
  EXPECT_EQ(13u, data->service_data(eddystone).size());

  EXPECT_TRUE(ContainersEqual(bytes.view(8), data->service_data(eddystone)));
}

// Per CSS v9 Part A 1.1.1, "A packet or data block shall not contain more than one instance for
// each Service UUID data size". We enforce this by failing to parse AdvertisingData with UUIDs of a
// particular size which exceed the amount that can fit in one TLV field.
TEST(AdvertisingDataTest, TooManyUuidsOfSizeRejected) {
  // Space for the maximum # of 16 bit UUIDs + length + type fields.
  const uint64_t kMaxAllowed16BitUuidsSize = (2 + kMax16BitUuids * UUIDElemSize::k16Bit);
  // Space for one more UUID + type and length fields
  const uint64_t kExpectedBuffSize = kMaxAllowed16BitUuidsSize + (2 + UUIDElemSize::k16Bit);

  DynamicByteBuffer bytes(kExpectedBuffSize);
  uint64_t offset = 0;
  // Write first TLV field with maximum # of UUIDs
  bytes.Write(StaticByteBuffer{
      kMax16BitUuids * UUIDElemSize::k16Bit + 1,                  // Size byte
      static_cast<uint8_t>(DataType::kComplete16BitServiceUuids)  // Type byte
  });
  offset += 2;
  for (uint16_t i = 0; i < kMax16BitUuids; ++i) {
    UUID uuid(static_cast<uint16_t>(i + 'a'));
    bytes.Write(uuid.CompactView(), offset);
    offset += uuid.CompactSize();
  }
  // Verify that we successfully parse an AD with the maximum amount of 16 bit UUIDs
  AdvertisingData::ParseResult adv_result =
      AdvertisingData::FromBytes(bytes.view(/*pos=*/0, /*size=*/kMaxAllowed16BitUuidsSize));
  ASSERT_TRUE(adv_result.is_ok());
  EXPECT_EQ(kMax16BitUuids, adv_result->service_uuids().size());
  // Write second Complete 16 bit Service UUIDs TLV field with one more UUID
  bytes.Write(
      StaticByteBuffer{
          UUIDElemSize::k16Bit + 1,                                   // Size byte
          static_cast<uint8_t>(DataType::kComplete16BitServiceUuids)  // Type byte
      },
      offset);
  offset += 2;
  UUID uuid(static_cast<uint16_t>(kMax16BitUuids + 'a'));
  bytes.Write(uuid.CompactView(), offset);

  adv_result = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(adv_result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kUuidsMalformed, adv_result.error_value());
}

TEST(AdvertisingDataTest, InvalidTlvFormat) {
  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(DynamicByteBuffer());
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kInvalidTlvFormat, result.error_value());
}

TEST(AdvertisingDataTest, TxPowerLevelMalformed) {
  StaticByteBuffer service_data{/*length=*/0x01, static_cast<uint8_t>(DataType::kTxPowerLevel)};
  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(service_data);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kTxPowerLevelMalformed, result.error_value());
}

TEST(AdvertisingDataTest, UuidsMalformed) {
  StaticByteBuffer service_data{
      0x02,  // Length
      static_cast<uint8_t>(DataType::kComplete16BitServiceUuids),
      0x12  // The length of a valid 16-bit UUID byte array be a multiple of 2 (and 1 % 2 == 1).
  };
  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(service_data);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kUuidsMalformed, result.error_value());
}

TEST(AdvertisingDataTest, ManufacturerSpecificDataTooSmall) {
  StaticByteBuffer service_data{
      0x02,  // Length
      static_cast<uint8_t>(DataType::kManufacturerSpecificData),
      0x12  // Manufacturer-specific data must be at least 2 bytes
  };
  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(service_data);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kManufacturerSpecificDataTooSmall, result.error_value());
}

TEST(AdvertisingDataTest, DecodeServiceDataWithIncompleteUuid) {
  auto service_data =
      StaticByteBuffer(0x02,                                               // Length
                       static_cast<uint8_t>(DataType::kServiceData16Bit),  // Data type
                       0xAA  // First byte of incomplete UUID
      );

  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(service_data);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kServiceDataTooSmall, result.error_value());
}

TEST(AdvertisingDataTest, AppearanceMalformed) {
  StaticByteBuffer service_data{
      0x02,  // Length
      static_cast<uint8_t>(DataType::kAppearance),
      0x12  // Appearance is supposed to be 2 bytes
  };
  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(service_data);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(AdvertisingData::ParseError::kAppearanceMalformed, result.error_value());
}
TEST(AdvertisingDataTest, Equality) {
  AdvertisingData one, two;

  UUID gatt(kGattUuid);
  UUID eddy(kEddystoneUuid);

  // Service UUIDs
  EXPECT_EQ(two, one);
  EXPECT_TRUE(one.AddServiceUuid(gatt));
  EXPECT_NE(two, one);
  EXPECT_TRUE(two.AddServiceUuid(gatt));
  EXPECT_EQ(two, one);

  // Even when the bytes are the same but from different places
  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03, 0x04);
  auto same = CreateStaticByteBuffer(0x01, 0x02, 0x03, 0x04);
  EXPECT_TRUE(two.SetManufacturerData(0x0123, bytes.view()));
  EXPECT_NE(two, one);
  EXPECT_TRUE(one.SetManufacturerData(0x0123, same.view()));
  EXPECT_EQ(two, one);

  // When TX Power is different
  two.SetTxPower(-34);
  EXPECT_NE(two, one);
  one.SetTxPower(-30);
  EXPECT_NE(two, one);
  one.SetTxPower(-34);
  EXPECT_EQ(two, one);

  // Even if the fields were added in different orders
  AdvertisingData three, four;
  EXPECT_TRUE(three.AddServiceUuid(eddy));
  EXPECT_TRUE(three.AddServiceUuid(gatt));
  EXPECT_NE(three, four);

  EXPECT_TRUE(four.AddServiceUuid(gatt));
  EXPECT_TRUE(four.AddServiceUuid(eddy));
  EXPECT_EQ(three, four);
}

TEST(AdvertisingDataTest, Copy) {
  UUID gatt(kGattUuid);
  UUID eddy(kEddystoneUuid);
  StaticByteBuffer<kRandomDataSize> rand_data;
  rand_data.FillWithRandomBytes();

  AdvertisingData source;
  EXPECT_TRUE(source.AddUri("http://fuchsia.cl"));
  EXPECT_TRUE(source.AddUri("https://ru.st"));
  EXPECT_TRUE(source.SetManufacturerData(0x0123, rand_data.view()));
  EXPECT_TRUE(source.AddServiceUuid(gatt));
  EXPECT_TRUE(source.AddServiceUuid(eddy));

  AdvertisingData dest;
  source.Copy(&dest);

  EXPECT_EQ(source, dest);

  // Modifying the source shouldn't mess with the copy
  EXPECT_TRUE(source.SetLocalName("fuchsia"));
  EXPECT_FALSE(dest.local_name());

  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  EXPECT_TRUE(source.SetManufacturerData(0x0123, bytes.view()));
  EXPECT_TRUE(ContainersEqual(rand_data, dest.manufacturer_data(0x0123)));
}

TEST(AdvertisingDataTest, Move) {
  UUID gatt(kGattUuid);
  UUID eddy(kEddystoneUuid);
  StaticByteBuffer<kRandomDataSize> rand_data;
  rand_data.FillWithRandomBytes();

  UUID heart_rate_uuid(kHeartRateServiceUuid);
  int8_t tx_power = 18;          // arbitrary TX power
  uint16_t appearance = 0x4567;  // arbitrary appearance value
  AdvertisingData source;
  EXPECT_TRUE(source.SetLocalName("test"));
  source.SetTxPower(tx_power);
  source.SetAppearance(appearance);
  EXPECT_TRUE(source.AddUri("http://fuchsia.cl"));
  EXPECT_TRUE(source.AddUri("https://ru.st"));
  EXPECT_TRUE(source.SetManufacturerData(0x0123, rand_data.view()));
  EXPECT_TRUE(source.AddServiceUuid(gatt));
  EXPECT_TRUE(source.AddServiceUuid(eddy));
  EXPECT_TRUE(source.SetServiceData(heart_rate_uuid, rand_data.view()));

  auto verify_advertising_data = [&](const AdvertisingData& dest, const char* type) {
    SCOPED_TRACE(type);
    // Dest should have the data we set.
    EXPECT_EQ("test", dest.local_name().value());
    EXPECT_EQ(tx_power, dest.tx_power().value());
    EXPECT_EQ(appearance, dest.appearance().value());
    EXPECT_EQ(std::unordered_set<std::string>({"http://fuchsia.cl", "https://ru.st"}), dest.uris());
    EXPECT_TRUE(ContainersEqual(rand_data, dest.manufacturer_data(0x0123)));
    EXPECT_EQ(std::unordered_set<UUID>({gatt, eddy}), dest.service_uuids());
    EXPECT_TRUE(ContainersEqual(rand_data, dest.service_data(heart_rate_uuid)));
  };

  AdvertisingData move_constructed(std::move(source));

  // source should be empty.
  EXPECT_EQ(AdvertisingData(), source);
  verify_advertising_data(move_constructed, "move_constructed");

  AdvertisingData move_assigned{};
  move_assigned = std::move(move_constructed);
  EXPECT_EQ(AdvertisingData(), move_constructed);
  verify_advertising_data(move_assigned, "move_assigned");
}

TEST(AdvertisingDataTest, Uris) {
  // The encoding scheme is represented by the first UTF-8 code-point in the URI string. Per
  // https://www.bluetooth.com/specifications/assigned-numbers/uri-scheme-name-string-mapping/,
  // 0xBA is the highest code point corresponding to an encoding scheme. However, 0xBA > 0x7F, so
  // representing too-large encoding schemes (i.e. code-points > 0xBA) in UTF-8 requires two bytes.
  const uint8_t kLargestKnownSchemeByte1 = 0xC2, kLargestKnownSchemeByte2 = 0xBA;
  // These bytes represent the (valid) UTF-8 code point for the (unknown encoding scheme) U+00BB.
  const uint8_t kUnknownSchemeByte1 = 0xC2, kUnknownSchemeByte2 = 0xBB;
  auto bytes = CreateStaticByteBuffer(
      // Uri: "https://abc.xyz"
      0x0B, DataType::kURI, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z',
      // Empty URI should be ignored:
      0x01, DataType::kURI,
      // Uri: "flubs:abc"
      0x0B, DataType::kURI, 0x01, 'f', 'l', 'u', 'b', 's', ':', 'a', 'b', 'c',
      // Uri: "ms-settings-cloudstorage:flub"
      0x07, DataType::kURI, kLargestKnownSchemeByte1, kLargestKnownSchemeByte2, 'f', 'l', 'u', 'b',
      // Invalid URI should be ignored - UTF-8 U+00BB doesn't correspond to an encoding scheme.
      0x07, DataType::kURI, kUnknownSchemeByte1, kUnknownSchemeByte2, 'f', 'l', 'u', 'b',
      // Invalid URI should be ignored - UTF-8 U+0000 doesn't correspond to an encoding scheme.
      0x03, DataType::kURI, 0x00, 0x00);

  AdvertisingData::ParseResult data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.is_ok());

  auto uris = data->uris();
  EXPECT_EQ(3u, uris.size());

  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "https://abc.xyz") != uris.end());
  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "flubs:abc") != uris.end());
  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "ms-settings-cloudstorage:flub") != uris.end());
}

// Tests writing a fully populated |AdvertisingData| to
// an output buffer succeeds.
TEST(AdvertisingDataTest, WriteBlockSuccess) {
  AdvertisingData data;

  data.SetTxPower(4);
  data.SetAppearance(0x4567);
  EXPECT_TRUE(data.SetLocalName("fuchsia"));

  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  EXPECT_TRUE(data.SetManufacturerData(0x0123, bytes.view()));

  auto service_uuid = UUID(kId1As16);
  auto service_bytes = CreateStaticByteBuffer(0x01, 0x02);
  EXPECT_TRUE(data.AddServiceUuid(service_uuid));
  EXPECT_TRUE(data.SetServiceData(service_uuid, service_bytes.view()));

  EXPECT_TRUE(data.AddUri("http://fuchsia.cl"));

  DynamicByteBuffer write_buf(data.CalculateBlockSize());
  EXPECT_TRUE(data.WriteBlock(&write_buf, std::nullopt));

  auto expected_buf = CreateStaticByteBuffer(
      0x02, 0x0a, 0x04,                                      // tx_power_level_: 4
      0x03, 0x19, 0x67, 0x45,                                // appearance_: 0x4567
      0x08, 0x09, 0x66, 0x75, 0x63, 0x68, 0x73, 0x69, 0x61,  // local_name_: "fuchsia"
      0x06, 0xff, 0x23, 0x01, 0x01, 0x02, 0x03,              // manufacturer_data_
      0x05, 0x16, 0x12, 0x02, 0x01, 0x02,                    // service_data_
      0x0e, 0x24, 0x16, 0x2f, 0x2f, 0x66, 0x75, 0x63, 0x68, 0x73, 0x69, 0x61, 0x2e, 0x63, 0x6c,
      0x03, 0x02, 0x12, 0x02);  // uris_
  EXPECT_TRUE(ContainersEqual(expected_buf, write_buf));
}

// Tests writing |AdvertisingData| to an output buffer that
// is too small fails gracefully and returns early.
TEST(AdvertisingDataTest, WriteBlockSmallBufError) {
  AdvertisingData data;

  data.SetTxPower(4);
  data.SetAppearance(0x4567);
  EXPECT_TRUE(data.SetLocalName("fuchsia"));

  DynamicByteBuffer write_buf(data.CalculateBlockSize() - 1);
  // The buffer is too small. No write should occur, and should return false.
  EXPECT_FALSE(data.WriteBlock(&write_buf, std::nullopt));
}

// Tests writing a fully populated |AdvertisingData| with provided flags to
// an output buffer succeeds.
TEST(AdvertisingDataTest, WriteBlockWithFlagsSuccess) {
  AdvertisingData data;

  data.SetTxPower(4);
  data.SetAppearance(0x4567);
  EXPECT_TRUE(data.SetLocalName("fuchsia"));

  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  EXPECT_TRUE(data.SetManufacturerData(0x0123, bytes.view()));

  auto service_uuid = UUID(kId1As16);
  auto service_bytes = CreateStaticByteBuffer(0x01, 0x02);
  EXPECT_TRUE(data.AddServiceUuid(service_uuid));
  EXPECT_TRUE(data.SetServiceData(service_uuid, service_bytes.view()));

  EXPECT_TRUE(data.AddUri("http://fuchsia.cl"));

  DynamicByteBuffer write_buf(data.CalculateBlockSize(/*include_flags=*/true));
  EXPECT_TRUE(data.WriteBlock(&write_buf, AdvFlag::kLEGeneralDiscoverableMode));

  auto expected_buf = CreateStaticByteBuffer(
      0x02, 0x01, 0x02,                                      // flags: 2
      0x02, 0x0a, 0x04,                                      // tx_power_level_: 4
      0x03, 0x19, 0x67, 0x45,                                // appearance_: 0x4567
      0x08, 0x09, 0x66, 0x75, 0x63, 0x68, 0x73, 0x69, 0x61,  // local_name_: "fuchsia"
      0x06, 0xff, 0x23, 0x01, 0x01, 0x02, 0x03,              // manufacturer_data_
      0x05, 0x16, 0x12, 0x02, 0x01, 0x02,                    // service_data_
      0x0e, 0x24, 0x16, 0x2f, 0x2f, 0x66, 0x75, 0x63, 0x68, 0x73, 0x69, 0x61, 0x2e, 0x63, 0x6c,
      0x03, 0x02, 0x12, 0x02);  // uris_
  EXPECT_TRUE(ContainersEqual(expected_buf, write_buf));
}

TEST(AdvertisingDataTest, WriteBlockWithFlagsBufError) {
  AdvertisingData data;

  data.SetTxPower(6);
  EXPECT_TRUE(data.SetLocalName("Fuchsia"));
  data.SetAppearance(0x1234);

  DynamicByteBuffer write_buf(data.CalculateBlockSize(/*include_flags=*/true) - 1);
  EXPECT_FALSE(data.WriteBlock(&write_buf, AdvFlag::kLEGeneralDiscoverableMode));
}

// Adds `n_(consecutively_increasing)_uuids` to `input` and returns the "next" UUID in the sequence.
// UUIDs may wrap around - this is OK, as we only care that they are all distinct.
UUID AddNDistinctUuids(AdvertisingData& input,
                       std::variant<uint16_t, uint32_t, UInt128> starting_uuid, uint8_t n_uuids) {
  UUID next;
  for (uint8_t i = 0; true; ++i) {
    std::visit(
        [&](auto arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, UInt128>) {
            arg[0] += i;
            next = UUID(arg);
          } else {
            next = UUID(static_cast<T>(arg + i));
          }
        },
        starting_uuid);
    SCOPED_TRACE(bt_lib_cpp_string::StringPrintf("i: %du UUID: %s", i, bt_str(next)));
    if (i >= n_uuids) {
      return next;
    }
    EXPECT_TRUE(input.AddServiceUuid(next));
  }
}

TEST(AdvertisingDataTest, SetFieldsWithTooLongParameters) {
  AdvertisingData data;
  {
    // Use the https URI encoding scheme. This prefix will be compressed to one byte when encoded.
    std::string uri = "https:";
    uri += std::string(kMaxEncodedUriLength - 1, '.');
    EXPECT_TRUE(data.AddUri(uri));
    uri += '.';
    EXPECT_FALSE(data.AddUri(uri));
  }
  // Attempt to set slightly too long service data.
  {
    UUID two_byte_uuid{kHeartRateServiceUuid};
    DynamicByteBuffer long_data(kMaxEncodedServiceDataLength - 1);
    long_data.Fill(0xAB);
    EXPECT_FALSE(data.SetServiceData(two_byte_uuid, long_data));
    // An empty DynamicByteBuffer represents unset service data per the header.
    EXPECT_TRUE(ContainersEqual(DynamicByteBuffer(), data.service_data(two_byte_uuid)));
    // Now use a view that is just small enough to fit when encoded
    BufferView view = long_data.view(/*pos=*/0, /*size=*/long_data.size() - 1);
    EXPECT_TRUE(data.SetServiceData(two_byte_uuid, view));
    EXPECT_TRUE(ContainersEqual(view, data.service_data(two_byte_uuid)));
  }
  // Attempt to set slightly too long manufacturer data.
  {
    uint16_t manufacturer_id{0xABBA};
    DynamicByteBuffer long_data(kMaxManufacturerDataLength + 1);
    long_data.Fill(0xAB);
    EXPECT_FALSE(data.SetManufacturerData(manufacturer_id, long_data.view()));
    // An empty DynamicByteBuffer represents unset service data per the header.
    EXPECT_TRUE(ContainersEqual(DynamicByteBuffer(), data.manufacturer_data(manufacturer_id)));
    // Now use a view that is just small enough to fit when encoded
    BufferView view = long_data.view(/*pos=*/0, /*size=*/long_data.size() - 1);
    EXPECT_TRUE(data.SetManufacturerData(manufacturer_id, view));
    EXPECT_TRUE(ContainersEqual(view, data.manufacturer_data(manufacturer_id)));
  }
  // Ensure that service UUIDs are truncated when they do not fit.
  {
    uint16_t starting_16bit_uuid = 0x0001;
    UUID should_fail = AddNDistinctUuids(
        data, std::variant<uint16_t, uint32_t, UInt128>{starting_16bit_uuid}, kMax16BitUuids);
    EXPECT_FALSE(data.AddServiceUuid(should_fail));
    EXPECT_TRUE(data.service_uuids().find(should_fail) == data.service_uuids().end());

    // This value must not fit in a 16 bit number in order to count as a "32 bit" UUID
    uint32_t starting_32bit_uuid = std::numeric_limits<uint16_t>::max() + 1;
    should_fail = AddNDistinctUuids(
        data, std::variant<uint16_t, uint32_t, UInt128>{starting_32bit_uuid}, kMax32BitUuids);
    EXPECT_FALSE(data.AddServiceUuid(should_fail));
    EXPECT_TRUE(data.service_uuids().find(should_fail) == data.service_uuids().end());

    UInt128 starting_128bit_uuid = {0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                                    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB};
    should_fail = AddNDistinctUuids(
        data, std::variant<uint16_t, uint32_t, UInt128>{starting_128bit_uuid}, kMax128BitUuids);
    EXPECT_FALSE(data.AddServiceUuid(should_fail));
    EXPECT_TRUE(data.service_uuids().find(should_fail) == data.service_uuids().end());
  }
  // Ensures names exceeding kMaxNameLength are rejected.
  {
    std::string name_that_fits(kMaxNameLength, 'a');
    std::string too_long_name(kMaxNameLength + 1, 'b');
    EXPECT_TRUE(data.SetLocalName(name_that_fits));
    EXPECT_EQ(name_that_fits, data.local_name());
    EXPECT_FALSE(data.SetLocalName(too_long_name));
    EXPECT_EQ(name_that_fits, data.local_name());
  }
  // Write the data out to ensure no assertions are triggered
  DynamicByteBuffer block(data.CalculateBlockSize());
  EXPECT_TRUE(data.WriteBlock(&block, std::nullopt));
}

// Tests that even when the maximum number of distinct UUIDs for a certain size have been added to
// an AD, we do not reject additional UUIDs that are duplicates of already-added UUIDs.
TEST(AdvertisingDataTest, AddDuplicateServiceUuidsWhenFullSucceeds) {
  AdvertisingData data;
  uint16_t starting_16bit_uuid = 0x0001;
  UUID should_fail = AddNDistinctUuids(
      data, std::variant<uint16_t, uint32_t, UInt128>{starting_16bit_uuid}, kMax16BitUuids);
  // Verify that adding another distinct UUID fails - i.e. we are at the limit.
  EXPECT_FALSE(data.AddServiceUuid(should_fail));
  EXPECT_TRUE(data.service_uuids().find(should_fail) == data.service_uuids().end());
  // Verify that we are notified of success when adding an existing UUID
  EXPECT_TRUE(data.AddServiceUuid(UUID(starting_16bit_uuid)));
}
}  // namespace
}  // namespace bt
