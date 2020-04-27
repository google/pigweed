// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packet.h"

#include <gtest/gtest.h>

namespace bt {
namespace sm {
namespace {

TEST(SMP_PacketTest, ParseValidPacket) {
  auto kValidPacket = CreateStaticByteBuffer(kPairingFailed, ErrorCode::kEncryptionKeySize);
  ByteBufferPtr valid_packet_ptr = std::make_unique<DynamicByteBuffer>(kValidPacket);
  fit::result<ValidPacketReader, ErrorCode> maybe_reader =
      ValidPacketReader::ParseSdu(valid_packet_ptr, kLEMTU);
  ASSERT_TRUE(maybe_reader.is_ok());
  ValidPacketReader reader = maybe_reader.value();
  ASSERT_EQ(reader.code(), kPairingFailed);
  ErrorCode payload = reader.payload<ErrorCode>();
  ASSERT_EQ(payload, ErrorCode::kEncryptionKeySize);
}

TEST(SMP_PacketTest, EmptyPacketGivesError) {
  ByteBufferPtr empty_packet_ptr = std::make_unique<DynamicByteBuffer>();
  fit::result<ValidPacketReader, ErrorCode> maybe_reader =
      ValidPacketReader::ParseSdu(empty_packet_ptr, kLEMTU);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error();
  ASSERT_EQ(ecode, ErrorCode::kInvalidParameters);
}

TEST(SMP_PacketTest, PacketLengthGreaterThanMtuGivesError) {
  ByteBufferPtr long_packet_ptr = std::make_unique<DynamicByteBuffer>(kLEMTU + 1);
  fit::result<ValidPacketReader, ErrorCode> maybe_reader =
      ValidPacketReader::ParseSdu(long_packet_ptr, kLEMTU);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error();
  ASSERT_EQ(ecode, ErrorCode::kInvalidParameters);
}

TEST(SMP_PacketTest, UnknownSMPCodeGivesError) {
  auto kUnknownCodePacket = CreateStaticByteBuffer(0xFF,  // Not a valid SMP packet header code.
                                                   ErrorCode::kEncryptionKeySize);
  ByteBufferPtr unknown_code_packet_ptr = std::make_unique<DynamicByteBuffer>(kUnknownCodePacket);
  fit::result<ValidPacketReader, ErrorCode> maybe_reader =
      ValidPacketReader::ParseSdu(unknown_code_packet_ptr, kLEMTU);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error();
  ASSERT_EQ(ecode, ErrorCode::kCommandNotSupported);
}

TEST(SMP_PacketTest, PayloadSizeDoesNotMatchHeaderGivesError) {
  // The PairingFailed code is expected to have a one byte error code as payload, not three bytes.
  auto kMalformedPacket = CreateStaticByteBuffer(kPairingFailed, 0x01, 0x01, 0x01);
  ByteBufferPtr malformed_packet_ptr = std::make_unique<DynamicByteBuffer>(kMalformedPacket);
  fit::result<ValidPacketReader, ErrorCode> maybe_reader =
      ValidPacketReader::ParseSdu(malformed_packet_ptr, kLEMTU);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error();
  ASSERT_EQ(ecode, ErrorCode::kInvalidParameters);
}

}  // namespace
}  // namespace sm
}  // namespace bt
