// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/sm/packet.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

namespace bt::sm {
namespace {

TEST(PacketTest, ParseValidPacket) {
  StaticByteBuffer kValidPacket(kPairingFailed, ErrorCode::kEncryptionKeySize);
  ByteBufferPtr valid_packet_ptr =
      std::make_unique<DynamicByteBuffer>(kValidPacket);
  fit::result<ErrorCode, ValidPacketReader> maybe_reader =
      ValidPacketReader::ParseSdu(valid_packet_ptr);
  ASSERT_TRUE(maybe_reader.is_ok());
  ValidPacketReader reader = maybe_reader.value();
  ASSERT_EQ(reader.code(), kPairingFailed);
  ErrorCode payload = reader.payload<ErrorCode>();
  ASSERT_EQ(payload, ErrorCode::kEncryptionKeySize);
}

TEST(PacketTest, EmptyPacketGivesError) {
  ByteBufferPtr empty_packet_ptr = std::make_unique<DynamicByteBuffer>();
  fit::result<ErrorCode, ValidPacketReader> maybe_reader =
      ValidPacketReader::ParseSdu(empty_packet_ptr);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error_value();
  ASSERT_EQ(ecode, ErrorCode::kInvalidParameters);
}

TEST(PacketTest, UnknownSMPCodeGivesError) {
  StaticByteBuffer kUnknownCodePacket(
      0xFF,  // Not a valid SMP packet header code.
      ErrorCode::kEncryptionKeySize);
  ByteBufferPtr unknown_code_packet_ptr =
      std::make_unique<DynamicByteBuffer>(kUnknownCodePacket);
  fit::result<ErrorCode, ValidPacketReader> maybe_reader =
      ValidPacketReader::ParseSdu(unknown_code_packet_ptr);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error_value();
  ASSERT_EQ(ecode, ErrorCode::kCommandNotSupported);
}

// This tests a case where the `size_t` packet size was stored into a `uint8_t`.
// If the packet size modulo 2^8 was 0, the length would overflow the `uint8_t`
// and the packet would be incorrectly recognized as having 0 length, leading to
// the wrong error being returned (and logged).
TEST(PacketTest, Mod256Equals0LengthPacketGivesCorrectError) {
  constexpr size_t k2ToThe8Size = 256;
  Code kInvalidSmpCode = 0xFF;
  StaticByteBuffer<k2ToThe8Size> unfortunately_sized_packet;
  PacketWriter(kInvalidSmpCode, &unfortunately_sized_packet);
  ByteBufferPtr p =
      std::make_unique<DynamicByteBuffer>(unfortunately_sized_packet);
  fit::result<ErrorCode, ValidPacketReader> maybe_reader =
      ValidPacketReader::ParseSdu(p);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error_value();
  ASSERT_EQ(ecode, ErrorCode::kCommandNotSupported);
}

TEST(PacketTest, PayloadSizeDoesNotMatchHeaderGivesError) {
  // The PairingFailed code is expected to have a one byte error code as
  // payload, not three bytes.
  StaticByteBuffer kMalformedPacket(kPairingFailed, 0x01, 0x01, 0x01);
  ByteBufferPtr malformed_packet_ptr =
      std::make_unique<DynamicByteBuffer>(kMalformedPacket);
  fit::result<ErrorCode, ValidPacketReader> maybe_reader =
      ValidPacketReader::ParseSdu(malformed_packet_ptr);
  ASSERT_TRUE(maybe_reader.is_error());
  ErrorCode ecode = maybe_reader.error_value();
  ASSERT_EQ(ecode, ErrorCode::kInvalidParameters);
}

}  // namespace
}  // namespace bt::sm
