// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "uuid.h"

#include <endian.h>

#include <cinttypes>

#include "pw_string/format.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt {
namespace {

// Format string that can be passed to sscanf. This allows sscanf to convert
// each octet into a uint8_t.
constexpr char kScanUuidFormatString[] =
    "%2" SCNx8 "%2" SCNx8 "%2" SCNx8 "%2" SCNx8
    "-"
    "%2" SCNx8 "%2" SCNx8
    "-"
    "%2" SCNx8 "%2" SCNx8
    "-"
    "%2" SCNx8 "%2" SCNx8
    "-"
    "%2" SCNx8 "%2" SCNx8 "%2" SCNx8 "%2" SCNx8 "%2" SCNx8 "%2" SCNx8;

// Parses the contents of a |uuid_string| and returns the result in |out_bytes|.
// Returns false if |uuid_string| does not represent a valid UUID.
// TODO(armansito): After having used UUID in camel-case words all over the
// place, I've decided that it sucks. I'm explicitly naming this using the
// "Uuid" style as a reminder to fix style elsewhere.
bool ParseUuidString(const std::string& uuid_string, UInt128* out_bytes) {
  BT_DEBUG_ASSERT(out_bytes);

  if (uuid_string.length() == 4) {
    // Possibly a 16-bit short UUID, parse it in context of the Base UUID.
    return ParseUuidString("0000" + uuid_string + "-0000-1000-8000-00805F9B34FB", out_bytes);
  }

  // This is a 36 character string, including 4 "-" characters and two
  // characters for each of the 16-octets that form the 128-bit UUID.
  if (uuid_string.length() != 36)
    return false;

  int result = std::sscanf(uuid_string.c_str(), kScanUuidFormatString, out_bytes->data() + 15,
                           out_bytes->data() + 14, out_bytes->data() + 13, out_bytes->data() + 12,
                           out_bytes->data() + 11, out_bytes->data() + 10, out_bytes->data() + 9,
                           out_bytes->data() + 8, out_bytes->data() + 7, out_bytes->data() + 6,
                           out_bytes->data() + 5, out_bytes->data() + 4, out_bytes->data() + 3,
                           out_bytes->data() + 2, out_bytes->data() + 1, out_bytes->data());

  return (result > 0) && (static_cast<size_t>(result) == out_bytes->size());
}

}  // namespace

bool UUID::FromBytes(const ByteBuffer& bytes, UUID* out_uuid) {
  switch (bytes.size()) {
    case UUIDElemSize::k16Bit: {
      uint16_t dst;
      memcpy(&dst, bytes.data(), sizeof(dst));
      *out_uuid = UUID(le16toh(dst));
      return true;
    }
    case UUIDElemSize::k32Bit: {
      uint32_t dst;
      memcpy(&dst, bytes.data(), sizeof(dst));
      *out_uuid = UUID(le32toh(dst));
      return true;
    }
    case UUIDElemSize::k128Bit: {
      UInt128 dst;
      memcpy(dst.data(), bytes.data(), sizeof(dst));
      *out_uuid = UUID(dst);
      return true;
    }
  }

  return false;
}

UUID::UUID(const ByteBuffer& bytes) {
  bool result = FromBytes(bytes, this);
  BT_ASSERT_MSG(result, "|bytes| must contain a 16, 32, or 128-bit UUID");
}

bool UUID::operator==(const UUID& uuid) const { return value_ == uuid.value_; }

bool UUID::operator==(uint16_t uuid16) const {
  if (type_ == Type::k16Bit)
    return uuid16 == ValueAs16Bit();

  // Quick conversion is not possible; compare as two 128-bit UUIDs.
  return *this == UUID(uuid16);
}

bool UUID::operator==(uint32_t uuid32) const {
  if (type_ != Type::k128Bit)
    return uuid32 == ValueAs32Bit();

  // Quick conversion is not possible; compare as two 128-bit UUIDs.
  return *this == UUID(uuid32);
}

bool UUID::operator==(const UInt128& uuid128) const { return value_ == uuid128; }

bool UUID::CompareBytes(const ByteBuffer& bytes) const {
  UUID other;
  if (!FromBytes(bytes, &other)) {
    return false;
  }
  return *this == other;
}

std::string UUID::ToString() const {
  char out[sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")];
  pw::StatusWithSize result = pw::string::Format(
      {out, sizeof(out)}, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      value_[15], value_[14], value_[13], value_[12], value_[11], value_[10], value_[9], value_[8],
      value_[7], value_[6], value_[5], value_[4], value_[3], value_[2], value_[1], value_[0]);
  BT_DEBUG_ASSERT(result.ok());
  return out;
}

UUIDElemSize UUID::CompactSize(bool allow_32bit) const {
  switch (type_) {
    case Type::k16Bit:
      return UUIDElemSize::k16Bit;
    case Type::k32Bit:
      if (allow_32bit)
        return UUIDElemSize::k32Bit;

      // Fall through if 32-bit UUIDs are not allowed.
      __FALLTHROUGH;
    case Type::k128Bit:
      return UUIDElemSize::k128Bit;
  };
  BT_PANIC("uuid type of %du is invalid", static_cast<uint8_t>(type_));
}

size_t UUID::ToBytes(MutableByteBuffer* bytes, bool allow_32bit) const {
  size_t size = CompactSize(allow_32bit);
  size_t offset = (size == UUIDElemSize::k128Bit) ? 0u : kBaseOffset;
  bytes->Write(value_.data() + offset, size);
  return size;
}

BufferView UUID::CompactView(bool allow_32bit) const {
  size_t size = CompactSize(allow_32bit);
  size_t offset = (size == UUIDElemSize::k128Bit) ? 0u : kBaseOffset;
  return BufferView(value_.data() + offset, size);
}

std::size_t UUID::Hash() const {
  static_assert(sizeof(value_) % sizeof(size_t) == 0);
  // Morally we'd like to assert this, but:
  //
  // 'alignof' applied to an expression is a GNU extension.
  //
  // static_assert(alignof(value_) % alignof(size_t) == 0);
  size_t hash = 0;
  for (size_t i = 0; i < (sizeof(value_) / sizeof(size_t)); i++) {
    hash ^= *reinterpret_cast<const size_t*>(value_.data() + (i * sizeof(size_t)));
  }
  return hash;
}

std::optional<uint16_t> UUID::As16Bit() const {
  std::optional<uint16_t> ret;
  if (type_ == Type::k16Bit) {
    ret = ValueAs16Bit();
  }
  return ret;
}

uint16_t UUID::ValueAs16Bit() const {
  BT_DEBUG_ASSERT(type_ == Type::k16Bit);

  return le16toh(*reinterpret_cast<const uint16_t*>(value_.data() + kBaseOffset));
}

uint32_t UUID::ValueAs32Bit() const {
  BT_DEBUG_ASSERT(type_ != Type::k128Bit);

  return le32toh(*reinterpret_cast<const uint32_t*>(value_.data() + kBaseOffset));
}

bool IsStringValidUuid(const std::string& uuid_string) {
  UInt128 bytes;
  return ParseUuidString(uuid_string, &bytes);
}

bool StringToUuid(const std::string& uuid_string, UUID* out_uuid) {
  BT_DEBUG_ASSERT(out_uuid);

  UInt128 bytes;
  if (!ParseUuidString(uuid_string, &bytes))
    return false;

  *out_uuid = UUID(bytes);
  return true;
}

}  // namespace bt
