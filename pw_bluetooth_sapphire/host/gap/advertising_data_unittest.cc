// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/advertising_data.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace gap {
namespace {

constexpr uint16_t kGattUuid = 0x1801;
constexpr uint16_t kEddystoneUuid = 0xFEAA;

constexpr uint16_t kId1As16 = 0x0212;
constexpr char kId1AsString[] = "00000212-0000-1000-8000-00805f9b34fb";
constexpr uint16_t kId2As16 = 0x1122;

constexpr char kId3AsString[] = "12341234-0000-1000-8000-00805f9b34fb";

constexpr size_t kRandomDataSize = 100;

TEST(GAP_AdvertisingDataTest, ReaderEmptyData) {
  BufferView empty;
  AdvertisingDataReader reader(empty);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(GAP_AdvertisingDataTest, MakeEmpty) {
  AdvertisingData data;

  EXPECT_EQ(0u, data.CalculateBlockSize());
}

TEST(GAP_AdvertisingDataTest, EncodeKnownURI) {
  AdvertisingData data;
  data.AddURI("https://abc.xyz");

  auto bytes =
      CreateStaticByteBuffer(0x0B, 0x24, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z');

  EXPECT_EQ(bytes.size(), data.CalculateBlockSize());
  DynamicByteBuffer block(data.CalculateBlockSize());
  data.WriteBlock(&block);
  EXPECT_TRUE(ContainersEqual(bytes, block));
}

TEST(GAP_AdvertisingDataTest, EncodeUnknownURI) {
  AdvertisingData data;
  data.AddURI("flubs:xyz");

  auto bytes =
      CreateStaticByteBuffer(0x0B, 0x24, 0x01, 'f', 'l', 'u', 'b', 's', ':', 'x', 'y', 'z');

  size_t block_size = data.CalculateBlockSize();
  EXPECT_EQ(bytes.size(), block_size);
  DynamicByteBuffer block(block_size);
  data.WriteBlock(&block);
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
  data.WriteBlock(&block);

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

  AdvertisingData data;

  EXPECT_TRUE(AdvertisingData::FromBytes(bytes, &data));

  EXPECT_EQ(3u, data.service_uuids().size());
  EXPECT_TRUE(data.local_name());
  EXPECT_EQ("Test💖", *(data.local_name()));
  EXPECT_TRUE(data.tx_power());
  EXPECT_EQ(-113, *(data.tx_power()));
}

TEST(GAP_AdvertisingDataTest, ParseFIDL) {
  fuchsia::bluetooth::le::AdvertisingDataDeprecated fidl_ad;

  // Confirming UTF-8 codepoints are working as well.
  fidl_ad.name = "Test💖";
  fidl_ad.service_uuids.push_back(kId1AsString);
  fidl_ad.service_uuids.push_back(kId3AsString);

  auto svc_data = fidl::VectorPtr<uint8_t>::New(4);
  for (size_t i = 0; i < svc_data->size(); i++) {
    svc_data->at(i) = static_cast<uint8_t>(i * 3);
  }

  fuchsia::bluetooth::le::ServiceDataEntry service_data_entry;
  service_data_entry.uuid = kId1AsString;
  service_data_entry.data = std::move(svc_data);
  fidl_ad.service_data.push_back(std::move(service_data_entry));

  AdvertisingData data;

  EXPECT_TRUE(AdvertisingData::FromFidl(fidl_ad, &data));

  ASSERT_EQ(2u, data.service_uuids().size());
  EXPECT_EQ("Test💖", *(data.local_name()));

  UUID uuid1(kId1As16);
  EXPECT_EQ(1u, data.service_data_uuids().size());
  EXPECT_EQ(4u, data.service_data(uuid1).size());

  EXPECT_FALSE(data.tx_power());
}

TEST(GAP_AdvertisingDataTest, ParseFIDLFailsWithMalformedUuid) {
  fuchsia::bluetooth::le::AdvertisingDataDeprecated fidl_ad;
  fidl_ad.service_uuids.push_back("12");

  AdvertisingData data;
  EXPECT_FALSE(AdvertisingData::FromFidl(fidl_ad, &data));
}

TEST(GAP_AdvertisingDataTest, ParseFIDLFailsWithMalformedServiceDataUuid) {
  fuchsia::bluetooth::le::AdvertisingDataDeprecated fidl_ad;

  auto svc_data = fidl::VectorPtr<uint8_t>::New(1);

  fuchsia::bluetooth::le::ServiceDataEntry service_data_entry;
  service_data_entry.uuid = "12";
  service_data_entry.data = std::move(svc_data);
  fidl_ad.service_data.push_back(std::move(service_data_entry));

  AdvertisingData data;
  EXPECT_FALSE(AdvertisingData::FromFidl(fidl_ad, &data));
}

TEST(GAP_AdvertisingDataTest, ManufacturerZeroLength) {
  auto bytes = CreateStaticByteBuffer(
      // Complete 16-bit UUIDs
      0x05, 0x03, 0x12, 0x02, 0x22, 0x11,
      // Manufacturer Data with no data
      0x03, 0xFF, 0x34, 0x12);

  AdvertisingData data;

  EXPECT_EQ(0u, data.manufacturer_data_ids().size());

  EXPECT_TRUE(AdvertisingData::FromBytes(bytes, &data));

  EXPECT_EQ(1u, data.manufacturer_data_ids().count(0x1234));
  EXPECT_EQ(0u, data.manufacturer_data(0x1234).size());
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

  AdvertisingData data;
  UUID eddystone((uint16_t)0xFEAA);

  EXPECT_EQ(0u, data.service_data_uuids().size());

  EXPECT_TRUE(AdvertisingData::FromBytes(bytes, &data));

  EXPECT_EQ(1u, data.service_data_uuids().size());
  EXPECT_EQ(13u, data.service_data(eddystone).size());

  EXPECT_TRUE(ContainersEqual(bytes.view(8), data.service_data(eddystone)));
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
  source.AddURI("http://fuchsia.cl");
  source.AddURI("https://ru.st");
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

  AdvertisingData source;
  source.AddURI("http://fuchsia.cl");
  source.AddURI("https://ru.st");
  source.SetManufacturerData(0x0123, rand_data.view());
  source.AddServiceUuid(gatt);
  source.AddServiceUuid(eddy);

  AdvertisingData dest = std::move(source);

  // source should be empty.
  EXPECT_EQ(AdvertisingData(), source);

  // Dest should have the data we set.
  EXPECT_EQ(std::unordered_set<std::string>({"http://fuchsia.cl", "https://ru.st"}), dest.uris());
  EXPECT_TRUE(ContainersEqual(rand_data, dest.manufacturer_data(0x0123)));

  EXPECT_EQ(std::unordered_set<UUID>({gatt, eddy}), dest.service_uuids());
}

TEST(GAP_AdvertisingDataTest, Uris) {
  auto bytes = CreateStaticByteBuffer(
      // Uri: "https://abc.xyz"
      0x0B, 0x24, 0x17, '/', '/', 'a', 'b', 'c', '.', 'x', 'y', 'z',
      // Uri: "flubs:abc"
      0x0B, 0x24, 0x01, 'f', 'l', 'u', 'b', 's', ':', 'a', 'b', 'c');

  AdvertisingData data;
  EXPECT_TRUE(AdvertisingData::FromBytes(bytes, &data));

  auto uris = data.uris();
  EXPECT_EQ(2u, uris.size());
  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "https://abc.xyz") != uris.end());
  EXPECT_TRUE(std::find(uris.begin(), uris.end(), "flubs:abc") != uris.end());
}

TEST(GAP_AdvertisingDataTest, ReaderMalformedData) {
  // TLV length exceeds the size of the payload
  auto bytes0 = CreateStaticByteBuffer(0x01);
  AdvertisingDataReader reader(bytes0);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());

  auto bytes = CreateStaticByteBuffer(0x05, 0x00, 0x00, 0x00, 0x00);
  reader = AdvertisingDataReader(bytes);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());

  // TLV length is 0. This is not considered malformed. Data should be valid but
  // should not return more data.
  bytes = CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x00, 0x00);
  reader = AdvertisingDataReader(bytes);
  EXPECT_TRUE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());

  // First field is valid, second field is not.
  DataType type;
  BufferView data;
  bytes = CreateStaticByteBuffer(0x02, 0x00, 0x00, 0x02, 0x00);
  reader = AdvertisingDataReader(bytes);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_FALSE(reader.GetNextField(&type, &data));

  // First field is valid, second field has length 0.
  bytes = CreateStaticByteBuffer(0x02, 0x00, 0x00, 0x00, 0x00);
  reader = AdvertisingDataReader(bytes);
  EXPECT_TRUE(reader.is_valid());
  EXPECT_TRUE(reader.HasMoreData());
  EXPECT_TRUE(reader.GetNextField(&type, &data));
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_FALSE(reader.GetNextField(&type, &data));
}

TEST(GAP_AdvertisingDataTest, ReaderParseFields) {
  auto bytes = CreateStaticByteBuffer(0x02, 0x01, 0x00, 0x05, 0x09, 'T', 'e', 's', 't');
  AdvertisingDataReader reader(bytes);
  EXPECT_TRUE(reader.is_valid());
  EXPECT_TRUE(reader.HasMoreData());

  DataType type;
  BufferView data;
  EXPECT_TRUE(reader.GetNextField(&type, &data));
  EXPECT_EQ(DataType::kFlags, type);
  EXPECT_EQ(1u, data.size());
  EXPECT_TRUE(ContainersEqual(CreateStaticByteBuffer(0x00), data));

  EXPECT_TRUE(reader.HasMoreData());
  EXPECT_TRUE(reader.GetNextField(&type, &data));
  EXPECT_EQ(DataType::kCompleteLocalName, type);
  EXPECT_EQ(4u, data.size());
  EXPECT_TRUE(ContainersEqual(std::string("Test"), data));

  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_FALSE(reader.GetNextField(&type, &data));
}

// Helper for computing the size of a string literal at compile time. sizeof()
// would have worked too but that counts the null character.
template <std::size_t N>
constexpr size_t StringSize(char const (&str)[N]) {
  return N - 1;
}

TEST(GAP_AdvertisingDataTest, WriteField) {
  constexpr char kValue0[] = "value zero";
  constexpr char kValue1[] = "value one";
  constexpr char kValue2[] = "value two";
  constexpr char kValue3[] = "value three";

  // Have just enough space for the first three values (+ 6 for 2 extra octets
  // for each TLV field).
  constexpr char kBufferSize = StringSize(kValue0) + StringSize(kValue1) + StringSize(kValue2) + 6;
  StaticByteBuffer<kBufferSize> buffer;

  AdvertisingDataWriter writer(&buffer);
  EXPECT_EQ(0u, writer.bytes_written());

  // We write malformed values here for testing purposes.
  EXPECT_TRUE(writer.WriteField(DataType::kFlags, BufferView(kValue0)));
  EXPECT_EQ(StringSize(kValue0) + 2, writer.bytes_written());

  EXPECT_TRUE(writer.WriteField(DataType::kShortenedLocalName, BufferView(kValue1)));
  EXPECT_EQ(StringSize(kValue0) + 2 + StringSize(kValue1) + 2, writer.bytes_written());

  // Trying to write kValue3 should fail because there isn't enough room left in
  // the buffer.
  EXPECT_FALSE(writer.WriteField(DataType::kCompleteLocalName, BufferView(kValue3)));

  // Writing kValue2 should fill up the buffer.
  EXPECT_TRUE(writer.WriteField(DataType::kCompleteLocalName, BufferView(kValue2)));
  EXPECT_FALSE(writer.WriteField(DataType::kCompleteLocalName, BufferView(kValue3)));
  EXPECT_EQ(buffer.size(), writer.bytes_written());

  // Verify the contents.
  DataType type;
  BufferView value;
  AdvertisingDataReader reader(buffer);
  EXPECT_TRUE(reader.is_valid());

  EXPECT_TRUE(reader.GetNextField(&type, &value));
  EXPECT_EQ(DataType::kFlags, type);
  EXPECT_EQ(kValue0, value.AsString());

  EXPECT_TRUE(reader.GetNextField(&type, &value));
  EXPECT_EQ(DataType::kShortenedLocalName, type);
  EXPECT_EQ(kValue1, value.AsString());

  EXPECT_TRUE(reader.GetNextField(&type, &value));
  EXPECT_EQ(DataType::kCompleteLocalName, type);
  EXPECT_EQ(kValue2, value.AsString());

  EXPECT_FALSE(reader.GetNextField(&type, &value));
}

}  // namespace
}  // namespace gap
}  // namespace bt
