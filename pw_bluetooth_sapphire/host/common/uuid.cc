// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "uuid.h"

#include <endian.h>
#include <zircon/assert.h>

#include <cinttypes>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/lib/fxl/strings/string_number_conversions.h"
#include "src/lib/fxl/strings/string_printf.h"

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

// Size in bytes of the three valid lengths of UUIDs
constexpr size_t k16BitSize = 2;
constexpr size_t k32BitSize = 4;
constexpr size_t k128BitSize = 16;

// Parses the contents of a |uuid_string| and returns the result in |out_bytes|.
// Returns false if |uuid_string| does not represent a valid UUID.
// TODO(armansito): After having used UUID in camel-case words all over the
// place, I've decided that it sucks. I'm explicitly naming this using the
// "Uuid" style as a reminder to fix style elsewhere.
bool ParseUuidString(const std::string& uuid_string, UInt128* out_bytes) {
  ZX_DEBUG_ASSERT(out_bytes);

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

// These constexpr static members need to be declared here for linkage as they
// get used in non-constexpr contexts as well:
// static
constexpr UInt128 UUID::kBaseUuid;
constexpr size_t UUID::kBaseOffset;

// static
bool UUID::FromBytes(const ByteBuffer& bytes, UUID* out_uuid) {
  switch (bytes.size()) {
    case k16BitSize:
      *out_uuid = UUID(le16toh(*reinterpret_cast<const uint16_t*>(bytes.data())));
      return true;
    case k32BitSize:
      *out_uuid = UUID(le32toh(*reinterpret_cast<const uint32_t*>(bytes.data())));
      return true;
    case k128BitSize:
      *out_uuid = UUID(*reinterpret_cast<const UInt128*>(bytes.data()));
      return true;
  }

  return false;
}

UUID::UUID(const ByteBuffer& bytes) {
  bool result = FromBytes(bytes, this);
  ZX_ASSERT_MSG(result, "|bytes| must contain a 16, 32, or 128-bit UUID");
}

UUID::UUID() : type_(Type::k128Bit) { value_.fill(0); }

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
  switch (bytes.size()) {
    case k16BitSize:
      return (*this == le16toh(*reinterpret_cast<const uint16_t*>(bytes.data())));
    case k32BitSize:
      return (*this == le32toh(*reinterpret_cast<const uint32_t*>(bytes.data())));
    case k128BitSize:
      return (*this == *reinterpret_cast<const UInt128*>(bytes.data()));
  }

  return false;
}

std::string UUID::ToString() const {
  return fxl::StringPrintf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                           value_[15], value_[14], value_[13], value_[12], value_[11], value_[10],
                           value_[9], value_[8], value_[7], value_[6], value_[5], value_[4],
                           value_[3], value_[2], value_[1], value_[0]);
}

size_t UUID::CompactSize(bool allow_32bit) const {
  switch (type_) {
    case Type::k16Bit:
      return k16BitSize;
    case Type::k32Bit:
      if (allow_32bit)
        return k32BitSize;

      // Fall through if 32-bit UUIDs are not allowed.
    case Type::k128Bit:
      return k128BitSize;
  };

  return 0;
}

size_t UUID::ToBytes(MutableByteBuffer* bytes, bool allow_32bit) const {
  size_t size = CompactSize(allow_32bit);
  size_t offset = (size == k128BitSize) ? 0u : kBaseOffset;
  bytes->Write(value_.data() + offset, size);
  return size;
}

const BufferView UUID::CompactView(bool allow_32bit) const {
  size_t size = CompactSize(allow_32bit);
  size_t offset = (size == k128BitSize) ? 0u : kBaseOffset;
  return BufferView(value_.data() + offset, size);
}

std::size_t UUID::Hash() const {
  ZX_DEBUG_ASSERT(sizeof(value_) % sizeof(size_t) == 0);
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
  ZX_DEBUG_ASSERT(type_ == Type::k16Bit);

  return le16toh(*reinterpret_cast<const uint16_t*>(value_.data() + kBaseOffset));
}

uint32_t UUID::ValueAs32Bit() const {
  ZX_DEBUG_ASSERT(type_ != Type::k128Bit);

  return le32toh(*reinterpret_cast<const uint32_t*>(value_.data() + kBaseOffset));
}

bool IsStringValidUuid(const std::string& uuid_string) {
  UInt128 bytes;
  return ParseUuidString(uuid_string, &bytes);
}

bool StringToUuid(const std::string& uuid_string, UUID* out_uuid) {
  ZX_DEBUG_ASSERT(out_uuid);

  UInt128 bytes;
  if (!ParseUuidString(uuid_string, &bytes))
    return false;

  *out_uuid = UUID(bytes);
  return true;
}

}  // namespace bt
