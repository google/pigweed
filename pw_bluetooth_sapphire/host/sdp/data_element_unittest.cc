// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sdp/data_element.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"

namespace bt::sdp {
namespace {

TEST(DataElementTest, CreateIsNull) {
  DataElement elem;
  EXPECT_EQ(DataElement::Type::kNull, elem.type());
  EXPECT_TRUE(elem.Get<std::nullptr_t>());
  EXPECT_EQ(nullptr, *elem.Get<std::nullptr_t>());

  auto expected = CreateStaticByteBuffer(0x00);
  DynamicByteBuffer buf(1);
  EXPECT_EQ(1u, elem.Write(&buf));
  EXPECT_TRUE(ContainersEqual(expected, buf));
}

TEST(DataElementTest, SetAndGet) {
  DataElement elem;

  elem.Set(uint8_t{5});

  EXPECT_TRUE(elem.Get<uint8_t>());
  EXPECT_EQ(5u, *elem.Get<uint8_t>());
  EXPECT_FALSE(elem.Get<int16_t>());

  elem.Set(std::string("Fuchsia💖"));

  EXPECT_FALSE(elem.Get<uint8_t>());
  EXPECT_TRUE(elem.Get<std::string>());
  EXPECT_EQ(std::string("Fuchsia💖"), *elem.Get<std::string>());
}

TEST(DataElementTest, Read) {
  auto buf = CreateStaticByteBuffer(
      0x25,  // Type (4: String) & Size (5: in an additional byte) = 0b00100 101
      0x0B,  // Bytes
      'F', 'u', 'c', 'h', 's', 'i', 'a', 0xF0, 0x9F, 0x92, 0x96,  // String
      0xDE, 0xAD, 0xBE, 0xEF  // Extra data (shouldn't be parsed)
  );

  DataElement elem;
  EXPECT_EQ(13u, DataElement::Read(&elem, buf));

  EXPECT_EQ(DataElement::Type::kString, elem.type());
  EXPECT_EQ(std::string("Fuchsia💖"), *elem.Get<std::string>());

  // Invalid - 0xDE: 0x11011 110 = 37 (invalid) + 6 (2 following byte size)
  EXPECT_EQ(0u, DataElement::Read(&elem, buf.view(13)));

  // elem shouldn't have been touched
  EXPECT_EQ(DataElement::Type::kString, elem.type());
  EXPECT_EQ(std::string("Fuchsia💖"), *elem.Get<std::string>());
}

TEST(DataElementTest, ReadUUID) {
  auto buf = CreateStaticByteBuffer(0x19,  // Type (3: UUID) & Size (1: two bytes) = 0b00011 001
                                    0x01, 0x00  // L2CAP
  );

  DataElement elem;
  EXPECT_EQ(3u, DataElement::Read(&elem, buf));
  EXPECT_EQ(DataElement::Type::kUuid, elem.type());
  EXPECT_EQ(UUID(uint16_t{0x0100}), *elem.Get<UUID>());

  auto buf2 = CreateStaticByteBuffer(0x1A,  // Type (3: UUID) & Size (2: four bytes) = 0b00011 010
                                     0x01, 0x02, 0x03, 0x04);

  EXPECT_EQ(5u, DataElement::Read(&elem, buf2));
  EXPECT_EQ(DataElement::Type::kUuid, elem.type());
  EXPECT_EQ(UUID(uint32_t{0x01020304}), *elem.Get<UUID>());

  auto buf3 = CreateStaticByteBuffer(0x1B,  // Type (3: UUID) & Size (3: eight bytes) = 0b00011 011
                                     0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02,
                                     0x03, 0x04, 0x01, 0x02, 0x03, 0x04);

  EXPECT_EQ(0u, DataElement::Read(&elem, buf3));

  auto buf4 = CreateStaticByteBuffer(0x1C,  // Type (3: UUID) & Size (3: eight bytes) = 0b00011 100
                                     0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                                     0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10);

  EXPECT_EQ(17u, DataElement::Read(&elem, buf4));
  EXPECT_EQ(DataElement::Type::kUuid, elem.type());
  // UInt128 in UUID is little-endian
  EXPECT_EQ(UUID({0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
                  0x03, 0x02, 0x01}),
            *elem.Get<UUID>());
}

TEST(DataElementTest, Write) {
  // This represents a plausible attribute_lists parameter of a
  // SDP_ServiceSearchAttributeResponse PDU for an SPP service.
  std::vector<DataElement> attribute_list;

  // SerialPort from Assigned Numbers
  std::vector<DataElement> service_class_list;
  service_class_list.emplace_back(DataElement(UUID(uint16_t{0x1101})));
  DataElement service_class_value(std::move(service_class_list));
  attribute_list.emplace_back(DataElement(kServiceClassIdList));
  attribute_list.emplace_back(std::move(service_class_value));

  // Protocol Descriptor List

  std::vector<DataElement> protocol_list_value;

  // ( L2CAP, PSM=RFCOMM )
  std::vector<DataElement> protocol_l2cap;
  protocol_l2cap.emplace_back(DataElement(protocol::kL2CAP));
  protocol_l2cap.emplace_back(DataElement(uint16_t{0x0003}));  // RFCOMM

  protocol_list_value.emplace_back(DataElement(std::move(protocol_l2cap)));

  // ( RFCOMM, CHANNEL=1 )
  std::vector<DataElement> protocol_rfcomm;
  protocol_rfcomm.push_back(DataElement(protocol::kRFCOMM));
  protocol_rfcomm.push_back(DataElement(uint8_t{1}));  // Server Channel = 1

  protocol_list_value.emplace_back(DataElement(std::move(protocol_rfcomm)));

  attribute_list.emplace_back(DataElement(kProtocolDescriptorList));
  attribute_list.emplace_back(DataElement(std::move(protocol_list_value)));

  // Bluetooth Profile Descriptor List
  std::vector<DataElement> profile_sequence_list;
  std::vector<DataElement> spp_sequence;
  spp_sequence.push_back(DataElement(UUID(uint16_t{0x1101})));
  spp_sequence.push_back(DataElement(uint16_t{0x0102}));

  profile_sequence_list.emplace_back(std::move(spp_sequence));

  attribute_list.push_back(DataElement(kBluetoothProfileDescriptorList));
  attribute_list.push_back((DataElement(std::move(profile_sequence_list))));

  DataElement attribute_lists_elem(std::move(attribute_list));

  // clang-format off
  auto expected = CreateStaticByteBuffer(
      0x35, 0x29,  // Sequence uint8 41 bytes
      0x09,        // uint16_t type
      UpperBits(kServiceClassIdList), LowerBits(kServiceClassIdList),
      0x35, 0x03,  // Sequence uint8 3 bytes
      0x19,        // UUID (16 bits)
      0x11, 0x01,  // Serial Port from assigned numbers
      0x09,        // uint16_t type
      UpperBits(kProtocolDescriptorList), LowerBits(kProtocolDescriptorList),
      0x35, 0x0F,  // Sequence uint8 15 bytes
      0x35, 0x06,  // Sequence uint8 6 bytes
      0x19,        // Type: UUID (16 bits)
      0x01, 0x00,  // L2CAP UUID
      0x09,        // Type: uint16_t
      0x00, 0x03,  // RFCOMM PSM
      0x35, 0x05,  // Sequence uint8 5 bytes
      0x19,        // Type: UUID (16 bits)
      0x00, 0x03,  // RFCOMM UUID
      0x08,        // Type: uint8_t
      0x01,        // RFCOMM Channel 1
      0x09,        // uint16_t type
      UpperBits(kBluetoothProfileDescriptorList),
      LowerBits(kBluetoothProfileDescriptorList),
      0x35, 0x08,  // Sequence uint8 8 bytes
      0x35, 0x06,  // Sequence uint8 6 bytes
      0x19,        // Type: UUID (16 bits)
      0x11, 0x01,  // 0x1101 (SPP)
      0x09,        // Type: uint16_t
      0x01, 0x02   // v1.2
  );
  // clang-format on

  DynamicByteBuffer block(43);

  size_t written = attribute_lists_elem.Write(&block);

  EXPECT_EQ(expected.size(), written);
  EXPECT_EQ(written, attribute_lists_elem.WriteSize());
  EXPECT_TRUE(ContainersEqual(expected, block));
}

TEST(DataElementTest, ReadSequence) {
  // clang-format off
  auto buf = CreateStaticByteBuffer(
      0x35, 0x08, // Sequence with 1 byte length (8)
      0x09, 0x00, 0x01,  // uint16_t: 1
      0x0A, 0x00, 0x00, 0x00, 0x02   // uint32_t: 2
  );
  // clang-format on

  DataElement elem;
  EXPECT_EQ(buf.size(), DataElement::Read(&elem, buf));
  EXPECT_EQ(DataElement::Type::kSequence, elem.type());
  auto *it = elem.At(0);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(1u, *it->Get<uint16_t>());

  it = elem.At(1);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(2u, *it->Get<uint32_t>());
}

TEST(DataElementTest, ReadNestedSeqeunce) {
  auto buf =
      CreateStaticByteBuffer(0x35, 0x1C,                    // Sequence uint8 28 bytes
                                                            // Sequence 0
                             0x35, 0x08,                    // Sequence uint8 8 bytes
                             0x09, 0x00, 0x00,              // Element: uint16_t (0)
                             0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
                             // Sequence 1
                             0x35, 0x10,                    // Sequence uint8 16 bytes
                             0x09, 0x00, 0x00,              // Element: uint16_t (0)
                             0x0A, 0xFE, 0xDB, 0xAC, 0x01,  // Element: uint32_t (0xFEDBAC01)
                             0x09, 0x00, 0x01,  // Handle: uint16_t (1 = kServiceClassIdList)
                             0x35, 0x03, 0x19, 0x11, 0x01  // Element: Sequence (3) { UUID(0x1101) }
      );

  DataElement elem;
  EXPECT_EQ(buf.size(), DataElement::Read(&elem, buf));
  EXPECT_EQ(DataElement::Type::kSequence, elem.type());
  auto *outer_it = elem.At(0);
  EXPECT_EQ(DataElement::Type::kSequence, outer_it->type());

  auto *it = outer_it->At(0);
  EXPECT_EQ(0u, *it->Get<uint16_t>());

  it = outer_it->At(1);
  EXPECT_EQ(0xfeedbeef, *it->Get<uint32_t>());

  outer_it = elem.At(1);
  EXPECT_EQ(DataElement::Type::kSequence, outer_it->type());

  it = outer_it->At(0);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(0u, *it->Get<uint16_t>());

  it = outer_it->At(1);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(0xfedbac01, *it->Get<uint32_t>());

  it = outer_it->At(2);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(1u, *it->Get<uint16_t>());

  it = outer_it->At(3);
  EXPECT_EQ(DataElement::Type::kSequence, it->type());

  auto inner_it = it->At(0);
  EXPECT_EQ(DataElement::Type::kUuid, inner_it->type());
}

TEST(DataElementTest, ToString) {
  EXPECT_EQ("Null", DataElement().ToString());
  EXPECT_EQ("Boolean(true)", DataElement(true).ToString());
  EXPECT_EQ("UnsignedInt:1(27)", DataElement(uint8_t{27}).ToString());
  EXPECT_EQ("SignedInt:4(-54321)", DataElement(int32_t{-54321}).ToString());
  EXPECT_EQ("UUID(00000100-0000-1000-8000-00805f9b34fb)", DataElement(protocol::kL2CAP).ToString());
  EXPECT_EQ("String(fuchsia💖)", DataElement(std::string("fuchsia💖")).ToString());
  std::vector<DataElement> strings;
  strings.emplace_back(std::string("hello"));
  strings.emplace_back(std::string("sapphire🔷"));
  EXPECT_EQ("Sequence { String(hello) String(sapphire🔷) }",
            DataElement(std::move(strings)).ToString());
  DataElement alts;
  strings.clear();
  strings.emplace_back(std::string("hello"));
  strings.emplace_back(std::string("sapphire🔷"));
  alts.SetAlternative(std::move(strings));
  EXPECT_EQ("Alternatives { String(hello) String(sapphire🔷) }", alts.ToString());
}

}  // namespace
}  // namespace bt::sdp
