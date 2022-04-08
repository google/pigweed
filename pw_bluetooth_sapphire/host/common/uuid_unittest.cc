// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "uuid.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace {

// Variants of 16-bit ID 180d
constexpr uint16_t kId1As16 = 0x180d;
constexpr uint32_t kId1As32 = 0x0000180d;
constexpr UInt128 kId1As128 = {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00,
                                0x00, 0x0d, 0x18, 0x00, 0x00}};
constexpr char kId1AsString[] = "0000180d-0000-1000-8000-00805f9b34fb";

// 16-bit ID for comparison
constexpr uint16_t kOther16BitId = 0x1800;

// Variants of 32-bit ID 0xdeadbeef
constexpr uint32_t kId2As32 = 0xdeadbeef;
constexpr UInt128 kId2As128 = {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00,
                                0x00, 0xef, 0xbe, 0xad, 0xde}};
constexpr char kId2AsString[] = "deadbeef-0000-1000-8000-00805f9b34fb";

constexpr UInt128 kId3As128 = {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                                0x0B,

                                // Make this part be the same as kId1* for the sake of testing.
                                0x0d, 0x18, 0x00, 0x00}};
constexpr char kId3AsString[] = "0000180d-0b0a-0908-0706-050403020100";

TEST(UUIDTest, 16Bit) {
  constexpr UUID uuid(kId1As16);

  // We perform each comparison twice, swapping the lhs and rhs, to test the
  // top-level equality operators.

  // Direct comparison with uint16_t.
  EXPECT_EQ(uuid, kId1As16);
  EXPECT_NE(uuid, kOther16BitId);
  EXPECT_EQ(kId1As16, uuid);
  EXPECT_NE(kOther16BitId, uuid);

  // Direct comparison with uint32_t.
  EXPECT_EQ(uuid, kId1As32);
  EXPECT_NE(uuid, kId2As32);
  EXPECT_EQ(kId1As32, uuid);
  EXPECT_NE(kId2As32, uuid);

  // Direct comparison with UInt128.
  EXPECT_EQ(kId1As128, uuid);
  EXPECT_NE(kId2As128, uuid);

  // Direct comparison with UUID.
  EXPECT_EQ(UUID(kId1As16), uuid);
  EXPECT_EQ(UUID(kId1As32), uuid);
  EXPECT_EQ(UUID(static_cast<uint16_t>(kId1As32)), uuid);
  EXPECT_EQ(UUID(kId1As128), uuid);
  EXPECT_NE(UUID(kOther16BitId), uuid);
  EXPECT_NE(UUID(kId2As32), uuid);
  EXPECT_NE(UUID(kId2As128), uuid);

  auto as16 = uuid.As16Bit();
  EXPECT_TRUE(as16);
  EXPECT_EQ(kId1As16, *as16);
}

TEST(UUIDTest, 32Bit) {
  constexpr UUID uuid(kId2As32);

  // Direct comparison with uint32_t.
  EXPECT_EQ(uuid, kId2As32);
  EXPECT_EQ(kId2As32, uuid);
  EXPECT_NE(uuid, kId1As32);
  EXPECT_NE(kId1As32, uuid);

  // Direct comparison with UInt128.
  EXPECT_EQ(kId2As128, uuid);
  EXPECT_NE(kId1As128, uuid);

  // Direct comparison with UUID.
  EXPECT_EQ(UUID(kId2As32), uuid);
  EXPECT_EQ(UUID(kId2As128), uuid);
  EXPECT_NE(UUID(kId1As16), uuid);
  EXPECT_NE(UUID(kId1As32), uuid);
  EXPECT_NE(UUID(kId1As128), uuid);

  EXPECT_FALSE(uuid.As16Bit());
}

TEST(UUIDTest, 128Bit) {
  constexpr UUID uuid(kId3As128);

  EXPECT_EQ(kId3As128, uuid);

  // 16-bit and 32-bit comparison should fail as the base-UUID portions do not
  // match.
  EXPECT_NE(kId1As16, uuid);
  EXPECT_NE(kId1As32, uuid);

  EXPECT_EQ(UUID(kId3As128), uuid);
  EXPECT_NE(UUID(kId1As128), uuid);

  EXPECT_FALSE(uuid.As16Bit());
}

TEST(UUIDTest, CompareBytes) {
  StaticByteBuffer kUuid16Bytes(0x0d, 0x18);
  StaticByteBuffer kUuid32Bytes(0x0d, 0x18, 0x00, 0x00);
  StaticByteBuffer kUuid128Bytes(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00,
                                 0x00, 0x0d, 0x18, 0x00, 0x00);

  constexpr UUID uuid(kId1As16);
  EXPECT_TRUE(uuid.CompareBytes(kUuid16Bytes));
  EXPECT_TRUE(uuid.CompareBytes(kUuid32Bytes));
  EXPECT_TRUE(uuid.CompareBytes(kUuid128Bytes));

  BufferView empty;
  EXPECT_FALSE(uuid.CompareBytes(empty));
}

TEST(UUIDTest, ToString) {
  EXPECT_EQ(kId1AsString, UUID(kId1As16).ToString());
  EXPECT_EQ(kId1AsString, UUID(kId1As32).ToString());
  EXPECT_EQ(kId1AsString, UUID(kId1As128).ToString());

  EXPECT_EQ(kId2AsString, UUID(kId2As32).ToString());
  EXPECT_EQ(kId2AsString, UUID(kId2As128).ToString());

  EXPECT_EQ(kId3AsString, UUID(kId3As128).ToString());
}

TEST(UUIDTest, IsStringValidUuid) {
  EXPECT_FALSE(IsStringValidUuid("0000180d00001000800000805f9b34fb"));
  EXPECT_FALSE(IsStringValidUuid("0000180d-0000-1000-8000000805f9b34fb"));
  EXPECT_FALSE(IsStringValidUuid("0000180d-0000-100008000-00805f9b34fb"));
  EXPECT_FALSE(IsStringValidUuid("0000180d-000001000-8000-00805f9b34fb"));
  EXPECT_FALSE(IsStringValidUuid("0000180d00000-1000-8000-00805f9b34fb"));
  EXPECT_FALSE(IsStringValidUuid("0000180d-0000-1000-8000-00805g9b34fb"));
  EXPECT_FALSE(IsStringValidUuid("000-180d-0000-1000-8000-00805f9b34fb"));

  // Combinations of lower and upper case characters should work.
  EXPECT_TRUE(IsStringValidUuid("0000180d-0000-1000-8000-00805f9b34fb"));
  EXPECT_TRUE(IsStringValidUuid("0000180D-0000-1000-8000-00805F9B34FB"));
  EXPECT_TRUE(IsStringValidUuid("0000180d-0000-1000-8000-00805F9b34fB"));
  EXPECT_TRUE(IsStringValidUuid(kId2AsString));
  EXPECT_TRUE(IsStringValidUuid(kId3AsString));
}

TEST(UUIDTest, StringToUuid) {
  UUID uuid;

  EXPECT_FALSE(StringToUuid("0000180d00001000800000805f9b34fb", &uuid));
  EXPECT_FALSE(StringToUuid("0000180d-0000-1000-8000000805f9b34fb", &uuid));
  EXPECT_FALSE(StringToUuid("0000180d-0000-100008000-00805f9b34fb", &uuid));
  EXPECT_FALSE(StringToUuid("0000180d-000001000-8000-00805f9b34fb", &uuid));
  EXPECT_FALSE(StringToUuid("0000180d00000-1000-8000-00805f9b34fb", &uuid));
  EXPECT_FALSE(StringToUuid("0000180d-0000-1000-8000-00805g9b34fb", &uuid));
  EXPECT_FALSE(StringToUuid("000-180d-0000-1000-8000-00805f9b34fb", &uuid));

  // Combinations of lower and upper case characters should work.
  EXPECT_TRUE(StringToUuid("0000180d-0000-1000-8000-00805f9b34fb", &uuid));
  EXPECT_EQ(kId1As16, uuid);
  EXPECT_TRUE(StringToUuid("0000180D-0000-1000-8000-00805F9B34FB", &uuid));
  EXPECT_EQ(kId1As16, uuid);
  EXPECT_TRUE(StringToUuid("0000180d-0000-1000-8000-00805F9b34fB", &uuid));
  EXPECT_EQ(kId1As16, uuid);

  EXPECT_TRUE(StringToUuid(kId2AsString, &uuid));
  EXPECT_EQ(kId2As32, uuid);

  EXPECT_TRUE(StringToUuid(kId3AsString, &uuid));
  EXPECT_EQ(kId3As128, uuid.value());
}

TEST(UUIDTest, StringToUuid16) {
  UUID uuid;

  EXPECT_FALSE(StringToUuid("0180d", &uuid));
  EXPECT_FALSE(StringToUuid("0000180d", &uuid));
  EXPECT_FALSE(StringToUuid("why", &uuid));
  EXPECT_FALSE(StringToUuid("d", &uuid));
  EXPECT_FALSE(StringToUuid("0x180d", &uuid));

  // Combinations of lower and upper case characters should work.
  EXPECT_TRUE(StringToUuid("180d", &uuid));
  EXPECT_EQ(kId1As16, uuid);
  EXPECT_TRUE(StringToUuid("180D", &uuid));
  EXPECT_EQ(kId1As16, uuid);
}

TEST(UUIDTest, FromBytes) {
  StaticByteBuffer kUuid16Bytes(0x0d, 0x18);
  StaticByteBuffer kUuid32Bytes(0x0d, 0x18, 0x00, 0x00);
  StaticByteBuffer kUuid128Bytes(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00,
                                 0x00, 0x0d, 0x18, 0x00, 0x00);

  StaticByteBuffer kInvalid0(0x0d);
  StaticByteBuffer kInvalid1(0x0d, 0x18, 0x00);
  BufferView kInvalid2;

  UUID uuid;

  EXPECT_FALSE(UUID::FromBytes(kInvalid0, &uuid));
  EXPECT_FALSE(UUID::FromBytes(kInvalid1, &uuid));
  EXPECT_FALSE(UUID::FromBytes(kInvalid2, &uuid));

  EXPECT_TRUE(UUID::FromBytes(kUuid16Bytes, &uuid));
  EXPECT_EQ(kId1As16, uuid);
  EXPECT_TRUE(UUID::FromBytes(kUuid32Bytes, &uuid));
  EXPECT_EQ(kId1As16, uuid);
  EXPECT_TRUE(UUID::FromBytes(kUuid128Bytes, &uuid));
  EXPECT_EQ(kId1As16, uuid);
}

TEST(UUIDTest, ByteConstructor) {
  const StaticByteBuffer kUuid16(0x0d, 0x18);
  UUID uuid(kUuid16);
  EXPECT_EQ(kId1As16, uuid);
}

TEST(UUIDTest, ByteConstructorAssertsOnInvalidInput) {
  const StaticByteBuffer kInvalidUuid(0x0d);
  EXPECT_DEATH_IF_SUPPORTED((UUID(kInvalidUuid)), ".*");
}

TEST(UUIDTest, CompactSize) {
  UUID direct(kId1As16);
  UUID fromstring;

  StringToUuid(kId1AsString, &fromstring);

  EXPECT_EQ(2u, direct.CompactSize());
  EXPECT_EQ(2u, fromstring.CompactSize());

  direct = UUID(kId2As32);
  StringToUuid(kId2AsString, &fromstring);

  EXPECT_EQ(4u, direct.CompactSize());
  EXPECT_EQ(4u, fromstring.CompactSize());
  EXPECT_EQ(16u, direct.CompactSize(/*allow_32bit=*/false));
  EXPECT_EQ(16u, fromstring.CompactSize(/*allow_32bit=*/false));

  direct = UUID(kId3As128);
  StringToUuid(kId3AsString, &fromstring);

  EXPECT_EQ(16u, direct.CompactSize());
  EXPECT_EQ(16u, fromstring.CompactSize());
}

TEST(UUIDTest, ToBytes16) {
  StaticByteBuffer kUuid16Bytes(0x0d, 0x18);

  UUID uuid(kId1As16);
  DynamicByteBuffer bytes(uuid.CompactSize());

  EXPECT_EQ(bytes.size(), uuid.ToBytes(&bytes));
  EXPECT_TRUE(ContainersEqual(kUuid16Bytes, bytes));

  uuid = UUID(kId1As32);

  EXPECT_EQ(bytes.size(), uuid.ToBytes(&bytes));
  EXPECT_TRUE(ContainersEqual(kUuid16Bytes, bytes));
}

TEST(UUIDTest, ToBytes32) {
  StaticByteBuffer kUuid32Bytes(0xef, 0xbe, 0xad, 0xde);

  UUID uuid(kId2As32);
  DynamicByteBuffer bytes(uuid.CompactSize());

  EXPECT_EQ(bytes.size(), uuid.ToBytes(&bytes));
  EXPECT_TRUE(ContainersEqual(kUuid32Bytes, bytes));

  StaticByteBuffer<16> bytes128;
  EXPECT_EQ(bytes128.size(), uuid.ToBytes(&bytes128, /*allow_32bit=*/false));
  EXPECT_TRUE(ContainersEqual(kId2As128, bytes128));
}

TEST(UUIDTest, CompactView16) {
  StaticByteBuffer kUuid16Bytes(0x0d, 0x18);

  UUID uuid(kId1As16);

  BufferView view = uuid.CompactView();
  EXPECT_TRUE(ContainersEqual(kUuid16Bytes, view));

  uuid = UUID(kId1As32);
  view = uuid.CompactView();
  EXPECT_TRUE(ContainersEqual(kUuid16Bytes, view));
}

TEST(UUIDTest, CompactView32) {
  StaticByteBuffer kUuid32Bytes(0xef, 0xbe, 0xad, 0xde);

  UUID uuid(kId2As32);

  BufferView view = uuid.CompactView();
  EXPECT_TRUE(ContainersEqual(kUuid32Bytes, view));

  view = uuid.CompactView(/*allow_32bit=*/false);
  EXPECT_TRUE(ContainersEqual(kId2As128, view));
}

TEST(UUIDTest, Hash) {
  constexpr UUID uuid1(kId3As128);
  constexpr UUID uuid2(kId3As128);

  EXPECT_EQ(uuid1, uuid2);
  EXPECT_EQ(uuid1.Hash(), uuid2.Hash());
}

}  // namespace
}  // namespace bt
