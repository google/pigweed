// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace {

constexpr uint16_t kGattUuid = 0x1801;
constexpr uint16_t kEddystoneUuid = 0xFEAA;
constexpr uint16_t kHeartRateServiceUuid = 0x180D;

constexpr uint16_t kId1As16 = 0x0212;
constexpr uint16_t kId2As16 = 0x1122;

constexpr size_t kRandomDataSize = 100;

TEST(GAP_AdvertisingDataTest, MakeEmpty) {
  AdvertisingData data;

  EXPECT_EQ(0u, data.CalculateBlockSize());
}

TEST(GAP_AdvertisingDataTest, EncodeKnownURI) {
  AdvertisingData data;
  EXPECT_TRUE(data.AddUri("https://abc.xyz"));

  auto bytes =
      CreateStaticByteBuffer(0x0B, 0x24, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z');

  EXPECT_EQ(bytes.size(), data.CalculateBlockSize());
  DynamicByteBuffer block(data.CalculateBlockSize());
  data.WriteBlock(&block, std::nullopt);
  EXPECT_TRUE(ContainersEqual(bytes, block));
}

TEST(GAP_AdvertisingDataTest, EncodeUnknownURI) {
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

TEST(GAP_AdvertisingDataTest, CompressServiceUUIDs) {
  AdvertisingData data;
  data.AddServiceUuid(UUID(kId1As16));
  data.AddServiceUuid(UUID(kId2As16));

  EXPECT_EQ(1 + 1 + (sizeof(uint16_t) * 2), data.CalculateBlockSize());

  auto bytes = CreateStaticByteBuffer(0x05, 0x02, 0x12, 0x02, 0x22, 0x11);

  size_t block_size = data.CalculateBlockSize();
  EXPECT_EQ(bytes.size(), block_size);
  DynamicByteBuffer block(block_size);
  data.WriteBlock(&block, std::nullopt);

  EXPECT_TRUE(ContainersEqual(bytes, block));
}

TEST(GAP_AdvertisingDataTest, ParseBlock) {
  auto bytes = CreateStaticByteBuffer(
      // Complete 16-bit UUIDs
      0x05, 0x03, 0x12, 0x02, 0x22, 0x11,
      // Incomplete list of 32-bit UUIDs
      0x05, 0x04, 0x34, 0x12, 0x34, 0x12,
      // Local name
      0x09, 0x09, 'T', 'e', 's', 't', 0xF0, 0x9F, 0x92, 0x96,
      // TX Power
      0x02, 0x0A, 0x8F);

  std::optional<AdvertisingData> data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.has_value());

  EXPECT_EQ(3u, data->service_uuids().size());
  EXPECT_TRUE(data->local_name());
  EXPECT_EQ("Test💖", *(data->local_name()));
  EXPECT_TRUE(data->tx_power());
  EXPECT_EQ(-113, *(data->tx_power()));
}

TEST(GAP_AdvertisingDataTest, ParseBlockUnknownDataType) {
  AdvertisingData expected_ad;
  uint8_t lower_byte = 0x12, upper_byte = 0x22;
  uint16_t uuid_value = (upper_byte << 8) + lower_byte;
  // The only field present in the expected AD is one complete 16-bit UUID.
  expected_ad.AddServiceUuid(UUID(uuid_value));

  auto bytes = StaticByteBuffer{
      // Complete 16-bit UUIDs
      0x03, 0x03, lower_byte, upper_byte,
      // 0x40, the second octet, is not a recognized DataType (see common/supplement_data.h).
      0x05, 0x40, 0x34, 0x12, 0x34, 0x12};
  std::optional<AdvertisingData> data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.has_value());

  // The second field of `bytes` was valid (in that its length byte matched its length), but its
  // Data Type was unknown, so it should be ignored (i.e. the only field in the `data` should be the
  // single 16-bit UUID, matching expected AD).
  EXPECT_EQ(expected_ad, *data);
}

TEST(GAP_AdvertisingDataTest, ManufacturerZeroLength) {
  auto bytes = CreateStaticByteBuffer(
      // Complete 16-bit UUIDs
      0x05, 0x03, 0x12, 0x02, 0x22, 0x11,
      // Manufacturer Data with no data
      0x03, 0xFF, 0x34, 0x12);

  EXPECT_EQ(0u, AdvertisingData().manufacturer_data_ids().size());

  std::optional<AdvertisingData> data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.has_value());

  EXPECT_EQ(1u, data->manufacturer_data_ids().count(0x1234));
  EXPECT_EQ(0u, data->manufacturer_data(0x1234).size());
}

TEST(GAP_AdvertisingDataTest, ServiceData) {
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

  std::optional<AdvertisingData> data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.has_value());

  UUID eddystone(uint16_t{0xFEAA});

  EXPECT_EQ(1u, data->service_data_uuids().size());
  EXPECT_EQ(13u, data->service_data(eddystone).size());

  EXPECT_TRUE(ContainersEqual(bytes.view(8), data->service_data(eddystone)));
}

TEST(GAP_AdvertisingDataTest, Equality) {
  AdvertisingData one, two;

  UUID gatt(kGattUuid);
  UUID eddy(kEddystoneUuid);

  // Service UUIDs
  EXPECT_EQ(two, one);
  one.AddServiceUuid(gatt);
  EXPECT_NE(two, one);
  two.AddServiceUuid(gatt);
  EXPECT_EQ(two, one);

  // Even when the bytes are the same but from different places
  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03, 0x04);
  auto same = CreateStaticByteBuffer(0x01, 0x02, 0x03, 0x04);
  two.SetManufacturerData(0x0123, bytes.view());
  EXPECT_NE(two, one);
  one.SetManufacturerData(0x0123, same.view());
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
  three.AddServiceUuid(eddy);
  three.AddServiceUuid(gatt);
  EXPECT_NE(three, four);

  four.AddServiceUuid(gatt);
  four.AddServiceUuid(eddy);
  EXPECT_EQ(three, four);
}

TEST(GAP_AdvertisingDataTest, Copy) {
  UUID gatt(kGattUuid);
  UUID eddy(kEddystoneUuid);
  StaticByteBuffer<kRandomDataSize> rand_data;
  rand_data.FillWithRandomBytes();

  AdvertisingData source;
  EXPECT_TRUE(source.AddUri("http://fuchsia.cl"));
  EXPECT_TRUE(source.AddUri("https://ru.st"));
  source.SetManufacturerData(0x0123, rand_data.view());
  source.AddServiceUuid(gatt);
  source.AddServiceUuid(eddy);

  AdvertisingData dest;
  source.Copy(&dest);

  EXPECT_EQ(source, dest);

  // Modifying the source shouldn't mess with the copy
  source.SetLocalName("fuchsia");
  EXPECT_FALSE(dest.local_name());

  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  source.SetManufacturerData(0x0123, bytes.view());
  EXPECT_TRUE(ContainersEqual(rand_data, dest.manufacturer_data(0x0123)));
}

TEST(GAP_AdvertisingDataTest, Move) {
  UUID gatt(kGattUuid);
  UUID eddy(kEddystoneUuid);
  StaticByteBuffer<kRandomDataSize> rand_data;
  rand_data.FillWithRandomBytes();

  UUID heart_rate_uuid(kHeartRateServiceUuid);
  int8_t tx_power = 18;          // arbitrary TX power
  uint16_t appearance = 0x4567;  // arbitrary appearance value
  AdvertisingData source;
  source.SetLocalName("test");
  source.SetTxPower(tx_power);
  source.SetAppearance(appearance);
  EXPECT_TRUE(source.AddUri("http://fuchsia.cl"));
  EXPECT_TRUE(source.AddUri("https://ru.st"));
  source.SetManufacturerData(0x0123, rand_data.view());
  source.AddServiceUuid(gatt);
  source.AddServiceUuid(eddy);
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

TEST(GAP_AdvertisingDataTest, Uris) {
  auto bytes = CreateStaticByteBuffer(
      // Uri: "https://abc.xyz"
      0x0B, 0x24, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z',
      // Uri: "flubs:abc"
      0x0B, 0x24, 0x01, 'f', 'l', 'u', 'b', 's', ':', 'a', 'b', 'c');

  std::optional<AdvertisingData> data = AdvertisingData::FromBytes(bytes);
  ASSERT_TRUE(data.has_value());

  auto uris = data->uris();
  EXPECT_EQ(2u, uris.size());
  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "https://abc.xyz") != uris.end());
  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "flubs:abc") != uris.end());
}

// Tests writing a fully populated |AdvertisingData| to
// an output buffer succeeds.
TEST(GAP_AdvertisingDataTest, WriteBlockSuccess) {
  AdvertisingData data;

  data.SetTxPower(4);
  data.SetAppearance(0x4567);
  data.SetLocalName("fuchsia");

  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  data.SetManufacturerData(0x0123, bytes.view());

  auto service_uuid = UUID(kId1As16);
  auto service_bytes = CreateStaticByteBuffer(0x01, 0x02);
  data.AddServiceUuid(service_uuid);
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
TEST(GAP_AdvertisingDataTest, WriteBlockSmallBufError) {
  AdvertisingData data;

  data.SetTxPower(4);
  data.SetAppearance(0x4567);
  data.SetLocalName("fuchsia");

  DynamicByteBuffer write_buf(data.CalculateBlockSize() - 1);
  // The buffer is too small. No write should occur, and should return false.
  EXPECT_FALSE(data.WriteBlock(&write_buf, std::nullopt));
}

// Tests writing a fully populated |AdvertisingData| with provided flags to
// an output buffer succeeds.
TEST(GAP_AdvertisingDataTest, WriteBlockWithFlagsSuccess) {
  AdvertisingData data;

  data.SetTxPower(4);
  data.SetAppearance(0x4567);
  data.SetLocalName("fuchsia");

  auto bytes = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  data.SetManufacturerData(0x0123, bytes.view());

  auto service_uuid = UUID(kId1As16);
  auto service_bytes = CreateStaticByteBuffer(0x01, 0x02);
  data.AddServiceUuid(service_uuid);
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

TEST(GAP_AdvertisingDataTest, WriteBlockWithFlagsBufError) {
  AdvertisingData data;

  data.SetTxPower(6);
  data.SetLocalName("Fuchsia");
  data.SetAppearance(0x1234);

  DynamicByteBuffer write_buf(data.CalculateBlockSize(/*include_flags=*/true) - 1);
  EXPECT_FALSE(data.WriteBlock(&write_buf, AdvFlag::kLEGeneralDiscoverableMode));
}

TEST(GAP_AdvertisingDataTest, SetFieldsWithTooLongParameters) {
  AdvertisingData data;
  // Use the https URI encoding scheme. This prefix will be compressed to one byte when encoded.
  std::string uri = "https:";
  uri += std::string(kMaxEncodedUriLength - 1, '.');
  EXPECT_TRUE(data.AddUri(uri));
  uri += '.';
  EXPECT_FALSE(data.AddUri(uri));

  // Attempt to set slightly too long advertising data.
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
}  // namespace
}  // namespace bt
