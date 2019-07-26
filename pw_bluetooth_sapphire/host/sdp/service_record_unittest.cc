// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sdp/service_record.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/data_element.h"

namespace bt {
namespace sdp {
namespace {

using SDP_ServiceRecordTest = ::testing::Test;

// Test: making a new record generates a UUID.
// Test: GetAttribute
// Test: HasAttribute
//   - returns true if attribute is there
//   - returns false if attribute is not there
// Test: RemoveAttribute
// Test: SetServiceClassUUIDs
//  - Sets the right attribute with the right format.
TEST_F(SDP_ServiceRecordTest, BasicFunctionality) {
  ServiceRecord record;

  record.SetHandle(kSDPHandle);

  EXPECT_EQ(kSDPHandle, record.handle());

  EXPECT_TRUE(record.HasAttribute(kServiceId));

  EXPECT_FALSE(record.HasAttribute(kServiceClassIdList));

  // This isn't a valid service class ID list:
  //  - ServiceDiscoveryServerServiceClassID
  //  - BrowseGroupDesciptorServiceClassID
  UUID sdp_id(uint16_t(0x1000));
  UUID group_id(uint16_t(0x1001));
  std::vector<UUID> service_class;
  service_class.push_back(sdp_id);
  service_class.emplace_back(group_id);

  record.SetServiceClassUUIDs(service_class);

  EXPECT_TRUE(record.HasAttribute(kServiceClassIdList));

  const DataElement& elem = record.GetAttribute(kServiceClassIdList);

  EXPECT_EQ(DataElement::Type::kSequence, elem.type());

  std::optional<std::vector<DataElement>> vec = elem.Get<std::vector<DataElement>>();

  EXPECT_TRUE(vec);
  EXPECT_EQ(2u, vec->size());
  EXPECT_EQ(sdp_id, *(vec->at(0).Get<UUID>()));
  EXPECT_EQ(group_id, *(vec->at(1).Get<UUID>()));

  record.RemoveAttribute(kServiceId);

  EXPECT_FALSE(record.HasAttribute(kServiceId));
}

// Test: GetAttributesInRange
//  - Returns any attributes that are present.
TEST_F(SDP_ServiceRecordTest, GetAttributesInRange) {
  ServiceRecord record;

  record.SetHandle(kSDPHandle);

  record.SetAttribute(0xf00d, DataElement());
  record.SetAttribute(0x0001, DataElement());
  record.SetAttribute(0xfeed, DataElement());

  auto attrs = record.GetAttributesInRange(kServiceRecordHandle, kServiceRecordHandle);

  EXPECT_EQ(1u, attrs.size());
  EXPECT_EQ(kServiceRecordHandle, *attrs.begin());

  // Get a copy of all elements
  attrs = record.GetAttributesInRange(0, 0xFFFF);

  EXPECT_EQ(5u, attrs.size());  // kServiceRecord, kServiceId, three added above
  EXPECT_NE(attrs.end(), attrs.find(kServiceRecordHandle));
  EXPECT_NE(attrs.end(), attrs.find(kServiceId));
  EXPECT_NE(attrs.end(), attrs.find(0xf00d));
  EXPECT_NE(attrs.end(), attrs.find(0x0001));
  EXPECT_NE(attrs.end(), attrs.find(0xfeed));
}

// Test: FindUUID
//  - Only returns true if all uuids are present
TEST_F(SDP_ServiceRecordTest, FindUUID) {
  ServiceRecord record;

  DataElement elem;
  elem.Set(UUID(uint16_t(0xfeaa)));
  record.SetAttribute(0xb001, std::move(elem));
  elem.Set(UUID(uint16_t(0xfeed)));
  record.SetAttribute(0xb002, std::move(elem));
  elem.Set(UUID(uint16_t(0xfeec)));
  record.SetAttribute(0xb003, std::move(elem));

  std::unordered_set<UUID> search_pattern;
  search_pattern.insert(UUID(uint16_t(0xfeaa)));

  EXPECT_TRUE(record.FindUUID(search_pattern));

  search_pattern.insert(UUID(uint16_t(0xfeec)));

  EXPECT_TRUE(record.FindUUID(search_pattern));

  search_pattern.insert(UUID(uint16_t(0xfeeb)));

  EXPECT_FALSE(record.FindUUID(search_pattern));
}

// Test: AddProtocolDescriptor
TEST_F(SDP_ServiceRecordTest, AddProtocolDescriptor) {
  ServiceRecord record;

  EXPECT_FALSE(record.HasAttribute(kProtocolDescriptorList));

  DataElement psm(uint16_t(0x0001));  // SDP PSM

  record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                               std::move(psm));

  // clang-format off
  auto expected = CreateStaticByteBuffer(
      0x35, 0x08, // Data Element Sequence (8 bytes)
      0x35, 0x06, // Data Element Sequence (6 bytes)
      0x19, // UUID (16 bits)
      0x01, 0x00, // L2CAP protocol UUID
      0x09, // uint16_t
      0x00, 0x01  // PSM=SDP
  );
  // clang-format on

  EXPECT_TRUE(record.HasAttribute(kProtocolDescriptorList));

  const DataElement& val = record.GetAttribute(kProtocolDescriptorList);
  DynamicByteBuffer block(val.WriteSize());
  val.Write(&block);

  EXPECT_EQ(expected.size(), block.size());
  EXPECT_TRUE(ContainersEqual(expected, block));

  record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kSDP, DataElement());

  EXPECT_TRUE(record.HasAttribute(kProtocolDescriptorList));

  // clang-format off
  auto expected_sdp = CreateStaticByteBuffer(
      0x35, 0x0D, // Data Element Sequence (13 bytes)
      0x35, 0x06, // Data Element Sequence (6 bytes)
      0x19, // UUID (16 bits)
      0x01, 0x00, // L2CAP protocol UUID
      0x09, // uint16_t
      0x00, 0x01,  // PSM=SDP
      0x35, 0x03, // Data Element Sequence (3 bytes)
      0x19, // UUID (16 bits)
      0x00, 0x01 // kSDP protocol
  );
  // clang-format on

  const DataElement& pdl = record.GetAttribute(kProtocolDescriptorList);
  DynamicByteBuffer block_sdp(pdl.WriteSize());
  pdl.Write(&block_sdp);

  EXPECT_EQ(expected_sdp.size(), block_sdp.size());
  EXPECT_TRUE(ContainersEqual(expected_sdp, block_sdp));

  record.AddProtocolDescriptor(1, protocol::kRFCOMM, DataElement());

  EXPECT_TRUE(record.HasAttribute(kAdditionalProtocolDescriptorList));

  // clang-format off
  auto expected_addl = CreateStaticByteBuffer(
      0x35, 0x07, // Data Element Sequence (AdditionalProtocolDescriptorLists)
      0x35, 0x05, // Data Element Sequence (ProtocolDescriptorList 1)
      0x35, 0x03, // Data Element Sequence Protocol List 1 Descriptor 0
      0x19, // UUID (16 bits)
      0x00, 0x03 // kRFCOMM protocol
  );
  // clang-format on

  const DataElement& apdl = record.GetAttribute(kAdditionalProtocolDescriptorList);
  DynamicByteBuffer block_addl(apdl.WriteSize());
  apdl.Write(&block_addl);

  EXPECT_EQ(expected_addl.size(), block_addl.size());
  EXPECT_TRUE(ContainersEqual(expected_addl, block_addl));
}

// Test: AddProfile
//  - Adds an attribute if it doesn't exist
//  - Appends to the attribute if it does exist
TEST_F(SDP_ServiceRecordTest, AddProfile) {
  ServiceRecord record;

  EXPECT_FALSE(record.HasAttribute(kBluetoothProfileDescriptorList));

  record.AddProfile(profile::kSerialPort, 2, 3);

  EXPECT_TRUE(record.HasAttribute(kBluetoothProfileDescriptorList));

  // clang-format off
  auto expected = CreateStaticByteBuffer(
      0x35, 0x08, // Data Element Sequence (8 bytes)
      0x35, 0x06, // Data Element Sequence (6 bytes)
      0x19, // UUID (16 bits)
      0x11, 0x01, // SerialPort protocol UUID
      0x09, // uint16_t
      0x02, 0x03  // 16 bit profile version number (major=2, minor=3)
  );
  // clang-format on

  const DataElement& val = record.GetAttribute(kBluetoothProfileDescriptorList);
  DynamicByteBuffer block(val.WriteSize());
  val.Write(&block);

  EXPECT_EQ(expected.size(), block.size());
  EXPECT_TRUE(ContainersEqual(expected, block));

  record.AddProfile(profile::kDialupNetworking, 4, 5);

  // clang-format off
  auto expected_dun = CreateStaticByteBuffer(
      0x35, 0x10, // Data Element Sequence (16 bytes)
      0x35, 0x06, // Data Element Sequence (6 bytes)
      0x19, // UUID (16 bits)
      0x11, 0x01, // SerialPort protocol UUID
      0x09, // uint16_t
      0x02, 0x03, // 16 bit profile version number (major=2, minor=3)
      0x35, 0x06, // Data Element Sequence (6 bytes)
      0x19, // UUID (16 bits)
      0x11, 0x03, // DUN UUID
      0x09, // uint16_t
      0x04, 0x05 // 16 bit profile version number (major=4, minor=5)
  );
  // clang-format on

  const DataElement& val_dun = record.GetAttribute(kBluetoothProfileDescriptorList);
  DynamicByteBuffer block_dun(val_dun.WriteSize());
  val_dun.Write(&block_dun);

  EXPECT_EQ(expected_dun.size(), block_dun.size());
  EXPECT_TRUE(ContainersEqual(expected_dun, block_dun));
}

// Test: AddInfo
//  - Requires at least one is set.
//  - Adds the right attributes to a set.
TEST_F(SDP_ServiceRecordTest, AddInfo) {
  ServiceRecord record;

  EXPECT_FALSE(record.HasAttribute(kLanguageBaseAttributeIdList));

  // Can't add with nothing specified.
  EXPECT_FALSE(record.AddInfo("en", "", "", ""));
  EXPECT_FALSE(record.HasAttribute(kLanguageBaseAttributeIdList));

  EXPECT_TRUE(record.AddInfo("en", "SDP", "💖", ""));

  EXPECT_TRUE(record.HasAttribute(kLanguageBaseAttributeIdList));
  const DataElement& val = record.GetAttribute(kLanguageBaseAttributeIdList);

  auto triplets = val.Get<std::vector<DataElement>>();
  EXPECT_TRUE(triplets);
  // They have to be triplets in this.
  EXPECT_TRUE(triplets->size() % 3 == 0);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, triplets->at(0).type());
  EXPECT_EQ(DataElement::Type::kUnsignedInt, triplets->at(1).type());
  EXPECT_EQ(DataElement::Type::kUnsignedInt, triplets->at(2).type());
  auto lang = triplets->at(0).Get<uint16_t>();
  EXPECT_TRUE(lang);
  EXPECT_EQ(0x656e, *lang);  // should be 'en' in ascii (but big-endian)

  auto encoding = triplets->at(1).Get<uint16_t>();
  EXPECT_TRUE(encoding);
  EXPECT_EQ(106, *encoding);  // should always be UTF-8

  auto base_attrid = triplets->at(2).Get<uint16_t>();
  EXPECT_TRUE(base_attrid);
  EXPECT_EQ(0x0100, *base_attrid);  // The primary language must be at 0x0100.

  EXPECT_TRUE(record.HasAttribute(*base_attrid + kServiceNameOffset));
  const DataElement& name_elem = record.GetAttribute(*base_attrid + kServiceNameOffset);
  auto name = name_elem.Get<std::string>();
  EXPECT_TRUE(name);
  EXPECT_EQ("SDP", *name);

  EXPECT_TRUE(record.HasAttribute(*base_attrid + kServiceDescriptionOffset));
  const DataElement& desc_elem = record.GetAttribute(*base_attrid + kServiceDescriptionOffset);
  auto desc = desc_elem.Get<std::string>();
  EXPECT_TRUE(desc);
  EXPECT_EQ("💖", *desc);

  EXPECT_FALSE(record.HasAttribute(*base_attrid + kProviderNameOffset));
};

}  // namespace
}  // namespace sdp
}  // namespace bt
