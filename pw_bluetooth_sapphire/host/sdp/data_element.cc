// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_element.h"

#include <endian.h>

#include <algorithm>
#include <set>
#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

// Returns true if |url| is a valid URI.
bool IsValidUrl(const std::string& url) {
  // Pulled from [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986).
  // See Section 2.2 for the set of reserved characters.
  // See Section 2.3 for the set of unreserved characters.
  constexpr char kValidUrlChars[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~!#$&'()*+,/:;=?@[]";
  return url.find_first_not_of(kValidUrlChars) == std::string::npos;
}

namespace bt::sdp {

namespace {

// Size Descriptor occupies the lowest 3 bits of the header byte.
// v5.0, Vol 3, Part B, Sec 3.3.
constexpr uint8_t kDataElementSizeTypeMask = 0x07;

DataElement::Size SizeToSizeType(size_t size) {
  switch (size) {
    case 1:
      return DataElement::Size::kOneByte;
    case 2:
      return DataElement::Size::kTwoBytes;
    case 4:
      return DataElement::Size::kFourBytes;
    case 8:
      return DataElement::Size::kEightBytes;
    case 16:
      return DataElement::Size::kSixteenBytes;
    default:
      BT_PANIC("invalid data element size: %zu", size);
  }
  return DataElement::Size::kNextFour;
}

size_t AggregateSize(const std::vector<DataElement>& aggregate) {
  size_t total_size = 0;
  for (const auto& elem : aggregate) {
    total_size += elem.WriteSize();
  }
  return total_size;
}

size_t WriteLength(MutableByteBuffer* buf, size_t length) {
  if (length <= std::numeric_limits<uint8_t>::max()) {
    uint8_t val = static_cast<uint8_t>(length);
    buf->Write(&val, sizeof(val));
    return sizeof(uint8_t);
  }

  if (length <= std::numeric_limits<uint16_t>::max()) {
    buf->WriteObj(htobe16(static_cast<uint16_t>(length)));
    return sizeof(uint16_t);
  }

  if (length <= std::numeric_limits<uint32_t>::max()) {
    buf->WriteObj(htobe32(static_cast<uint32_t>(length)));
    return sizeof(uint32_t);
  }

  return 0;
}

}  // namespace

DataElement::DataElement() : type_(Type::kNull), size_(Size::kOneByte) {}

DataElement::DataElement(const DataElement& other) : type_(other.type_), size_(other.size_) {
  switch (type_) {
    case Type::kNull:
      return;
    case Type::kUnsignedInt:
      uint_value_ = other.uint_value_;
      return;
    case Type::kBoolean:
    case Type::kSignedInt:
      int_value_ = other.int_value_;
      return;
    case Type::kUuid:
      uuid_ = other.uuid_;
      return;
    case Type::kString:
    case Type::kUrl:
      bytes_ = DynamicByteBuffer(other.bytes_);
      return;
    case Type::kSequence:
    case Type::kAlternative:
      for (const auto& it : other.aggregate_) {
        aggregate_.emplace_back(DataElement(it));
      }
      return;
  }
}

template <>
void DataElement::Set<uint8_t>(uint8_t value) {
  type_ = Type::kUnsignedInt;
  size_ = SizeToSizeType(sizeof(uint8_t));
  uint_value_ = value;
}

template <>
void DataElement::Set<uint16_t>(uint16_t value) {
  type_ = Type::kUnsignedInt;
  size_ = SizeToSizeType(sizeof(uint16_t));
  uint_value_ = value;
}

template <>
void DataElement::Set<uint32_t>(uint32_t value) {
  type_ = Type::kUnsignedInt;
  size_ = SizeToSizeType(sizeof(uint32_t));
  uint_value_ = value;
}

template <>
void DataElement::Set<uint64_t>(uint64_t value) {
  type_ = Type::kUnsignedInt;
  size_ = SizeToSizeType(sizeof(uint64_t));
  uint_value_ = value;
}

template <>
void DataElement::Set<int8_t>(int8_t value) {
  type_ = Type::kSignedInt;
  size_ = SizeToSizeType(sizeof(int8_t));
  int_value_ = value;
}

template <>
void DataElement::Set<int16_t>(int16_t value) {
  type_ = Type::kSignedInt;
  size_ = SizeToSizeType(sizeof(int16_t));
  int_value_ = value;
}

template <>
void DataElement::Set<int32_t>(int32_t value) {
  type_ = Type::kSignedInt;
  size_ = SizeToSizeType(sizeof(int32_t));
  int_value_ = value;
}

template <>
void DataElement::Set<int64_t>(int64_t value) {
  type_ = Type::kSignedInt;
  size_ = SizeToSizeType(sizeof(int64_t));
  int_value_ = value;
}

template <>
void DataElement::Set<bool>(bool value) {
  type_ = Type::kBoolean;
  size_ = Size::kOneByte;
  int_value_ = (value ? 1 : 0);
}

template <>
void DataElement::Set<std::nullptr_t>(std::nullptr_t) {
  type_ = Type::kNull;
  size_ = Size::kOneByte;
}

template <>
void DataElement::Set<UUID>(UUID value) {
  type_ = Type::kUuid;
  size_ = SizeToSizeType(value.CompactSize());
  uuid_ = value;
}

void DataElement::Set(const bt::DynamicByteBuffer& value) {
  type_ = Type::kString;
  SetVariableSize(value.size());
  bytes_ = DynamicByteBuffer(value);
}

void DataElement::Set(const std::string& value) {
  type_ = Type::kString;
  SetVariableSize(value.size());
  bytes_ = DynamicByteBuffer(value);
}

void DataElement::Set(std::vector<DataElement>&& value) {
  type_ = Type::kSequence;
  aggregate_ = std::move(value);
  SetVariableSize(AggregateSize(aggregate_));
}

void DataElement::SetUrl(const std::string& url) {
  if (!IsValidUrl(url)) {
    bt_log(WARN, "sdp", "Invalid URL in SetUrl: %s", url.c_str());
    return;
  }

  type_ = Type::kUrl;
  SetVariableSize(url.size());
  bytes_ = DynamicByteBuffer(url);
}

void DataElement::SetAlternative(std::vector<DataElement>&& items) {
  type_ = Type::kAlternative;
  aggregate_ = std::move(items);
  SetVariableSize(AggregateSize(aggregate_));
}

template <>
std::optional<uint8_t> DataElement::Get<uint8_t>() const {
  if (type_ == Type::kUnsignedInt && size_ == SizeToSizeType(sizeof(uint8_t))) {
    return static_cast<uint8_t>(uint_value_);
  }

  return std::nullopt;
}

template <>
std::optional<uint16_t> DataElement::Get<uint16_t>() const {
  if (type_ == Type::kUnsignedInt && size_ == SizeToSizeType(sizeof(uint16_t))) {
    return static_cast<uint16_t>(uint_value_);
  }

  return std::nullopt;
}

template <>
std::optional<uint32_t> DataElement::Get<uint32_t>() const {
  if (type_ == Type::kUnsignedInt && size_ == SizeToSizeType(sizeof(uint32_t))) {
    return static_cast<uint32_t>(uint_value_);
  }

  return std::nullopt;
}

template <>
std::optional<uint64_t> DataElement::Get<uint64_t>() const {
  if (type_ == Type::kUnsignedInt && size_ == SizeToSizeType(sizeof(uint64_t))) {
    return uint_value_;
  }

  return std::nullopt;
}

template <>
std::optional<int8_t> DataElement::Get<int8_t>() const {
  if (type_ == Type::kSignedInt && size_ == SizeToSizeType(sizeof(int8_t))) {
    return static_cast<int8_t>(int_value_);
  }

  return std::nullopt;
}

template <>
std::optional<int16_t> DataElement::Get<int16_t>() const {
  if (type_ == Type::kSignedInt && size_ == SizeToSizeType(sizeof(int16_t))) {
    return static_cast<int16_t>(int_value_);
  }

  return std::nullopt;
}

template <>
std::optional<int32_t> DataElement::Get<int32_t>() const {
  if (type_ == Type::kSignedInt && size_ == SizeToSizeType(sizeof(int32_t))) {
    return static_cast<int32_t>(int_value_);
  }

  return std::nullopt;
  ;
}

template <>
std::optional<int64_t> DataElement::Get<int64_t>() const {
  if (type_ == Type::kSignedInt && size_ == SizeToSizeType(sizeof(int64_t))) {
    return static_cast<int64_t>(int_value_);
  }

  return std::nullopt;
}

template <>
std::optional<bool> DataElement::Get<bool>() const {
  if (type_ != Type::kBoolean) {
    return std::nullopt;
  }

  return (int_value_ == 1);
}

template <>
std::optional<std::nullptr_t> DataElement::Get<std::nullptr_t>() const {
  if (type_ != Type::kNull) {
    return std::nullopt;
  }

  return nullptr;
}

template <>
std::optional<bt::DynamicByteBuffer> DataElement::Get<bt::DynamicByteBuffer>() const {
  if (type_ != Type::kString) {
    return std::nullopt;
  }

  return DynamicByteBuffer(bytes_);
}

template <>
std::optional<std::string> DataElement::Get<std::string>() const {
  if (type_ != Type::kString) {
    return std::nullopt;
  }

  return std::string(reinterpret_cast<const char*>(bytes_.data()), bytes_.size());
}

template <>
std::optional<UUID> DataElement::Get<UUID>() const {
  if (type_ != Type::kUuid) {
    return std::nullopt;
  }

  return uuid_;
}

template <>
std::optional<std::vector<DataElement>> DataElement::Get<std::vector<DataElement>>() const {
  if (type_ != Type::kSequence) {
    return std::nullopt;
  }

  std::vector<DataElement> aggregate_copy;
  for (const auto& it : aggregate_) {
    aggregate_copy.emplace_back(it.Clone());
  }

  return aggregate_copy;
}

std::optional<std::string> DataElement::GetUrl() const {
  if (type_ != Type::kUrl) {
    return std::nullopt;
  }

  return std::string(reinterpret_cast<const char*>(bytes_.data()), bytes_.size());
}

void DataElement::SetVariableSize(size_t length) {
  if (length <= std::numeric_limits<uint8_t>::max()) {
    size_ = Size::kNextOne;
  } else if (length <= std::numeric_limits<uint16_t>::max()) {
    size_ = Size::kNextTwo;
  } else {
    size_ = Size::kNextFour;
  }
}

size_t DataElement::Read(DataElement* elem, const ByteBuffer& buffer) {
  if (buffer.size() == 0) {
    return 0;
  }
  Type type_desc = static_cast<Type>(buffer[0] & kTypeMask);
  Size size_desc = static_cast<Size>(buffer[0] & kDataElementSizeTypeMask);
  size_t data_bytes = 0;
  size_t bytes_read = 1;
  BufferView cursor = buffer.view(bytes_read);
  switch (size_desc) {
    case DataElement::Size::kOneByte:
    case DataElement::Size::kTwoBytes:
    case DataElement::Size::kFourBytes:
    case DataElement::Size::kEightBytes:
    case DataElement::Size::kSixteenBytes:
      if (type_desc != Type::kNull) {
        data_bytes = (1 << static_cast<uint8_t>(size_desc));
      } else {
        data_bytes = 0;
      }
      break;
    case DataElement::Size::kNextOne: {
      if (cursor.size() < sizeof(uint8_t)) {
        return 0;
      }
      data_bytes = cursor.To<uint8_t>();
      bytes_read += sizeof(uint8_t);
      break;
    }
    case DataElement::Size::kNextTwo: {
      if (cursor.size() < sizeof(uint16_t)) {
        return 0;
      }
      data_bytes = betoh16(cursor.To<uint16_t>());
      bytes_read += sizeof(uint16_t);
      break;
    }
    case DataElement::Size::kNextFour: {
      if (cursor.size() < sizeof(uint32_t)) {
        return 0;
      }
      data_bytes = betoh32(cursor.To<uint32_t>());
      bytes_read += sizeof(uint32_t);
      break;
    }
  }
  cursor = buffer.view(bytes_read);
  if (cursor.size() < data_bytes) {
    return 0;
  }

  switch (type_desc) {
    case Type::kNull: {
      if (size_desc != Size::kOneByte) {
        return 0;
      }
      elem->Set(nullptr);
      return bytes_read + data_bytes;
    }
    case Type::kBoolean: {
      if (size_desc != Size::kOneByte) {
        return 0;
      }
      elem->Set(cursor.To<uint8_t>() != 0);
      return bytes_read + data_bytes;
    }
    case Type::kUnsignedInt: {
      if (size_desc == Size::kOneByte) {
        elem->Set(cursor.To<uint8_t>());
      } else if (size_desc == Size::kTwoBytes) {
        elem->Set(betoh16(cursor.To<uint16_t>()));
      } else if (size_desc == Size::kFourBytes) {
        elem->Set(betoh32(cursor.To<uint32_t>()));
      } else if (size_desc == Size::kEightBytes) {
        elem->Set(betoh64(cursor.To<uint64_t>()));
      } else {
        // TODO(jamuraa): support 128-bit uints
        // Invalid size.
        return 0;
      }
      return bytes_read + data_bytes;
    }
    case Type::kSignedInt: {
      if (size_desc == Size::kOneByte) {
        elem->Set(cursor.To<int8_t>());
      } else if (size_desc == Size::kTwoBytes) {
        elem->Set(betoh16(cursor.To<int16_t>()));
      } else if (size_desc == Size::kFourBytes) {
        elem->Set(betoh32(cursor.To<int32_t>()));
      } else if (size_desc == Size::kEightBytes) {
        elem->Set(betoh64(cursor.To<int64_t>()));
      } else {
        // TODO(jamuraa): support 128-bit uints
        // Invalid size.
        return 0;
      }
      return bytes_read + data_bytes;
    }
    case Type::kUuid: {
      if (size_desc == Size::kTwoBytes) {
        elem->Set(UUID(betoh16(cursor.To<uint16_t>())));
      } else if (size_desc == Size::kFourBytes) {
        elem->Set(UUID(betoh32(cursor.To<uint32_t>())));
      } else if (size_desc == Size::kSixteenBytes) {
        StaticByteBuffer<16> uuid_bytes;
        // UUID expects these to be in little-endian order.
        cursor.Copy(&uuid_bytes, 0, 16);
        std::reverse(uuid_bytes.mutable_data(), uuid_bytes.mutable_data() + 16);
        UUID uuid(uuid_bytes);
        elem->Set(uuid);
      } else {
        return 0;
      }
      return bytes_read + data_bytes;
    }
    case Type::kString: {
      if (static_cast<uint8_t>(size_desc) < 5) {
        return 0;
      }
      bt::DynamicByteBuffer str(data_bytes);
      str.Write(cursor.data(), data_bytes);
      elem->Set(str);
      return bytes_read + data_bytes;
    }
    case Type::kSequence:
    case Type::kAlternative: {
      if (static_cast<uint8_t>(size_desc) < 5) {
        return 0;
      }
      BufferView sequence_buf = cursor.view(0, data_bytes);
      size_t remaining = data_bytes;
      BT_DEBUG_ASSERT(sequence_buf.size() == data_bytes);

      std::vector<DataElement> seq;
      while (remaining > 0) {
        DataElement next;
        size_t used = Read(&next, sequence_buf.view(data_bytes - remaining));
        if (used == 0 || used > remaining) {
          return 0;
        }
        seq.push_back(std::move(next));
        remaining -= used;
      }
      BT_DEBUG_ASSERT(remaining == 0);
      if (type_desc == Type::kAlternative) {
        elem->SetAlternative(std::move(seq));
      } else {
        elem->Set(std::move(seq));
      }
      return bytes_read + data_bytes;
    }
    case Type::kUrl: {
      if (static_cast<uint8_t>(size_desc) < 5) {
        return 0;
      }
      std::string str(cursor.data(), cursor.data() + data_bytes);
      elem->SetUrl(str);
      return bytes_read + data_bytes;
    }
  }
  return 0;
}

size_t DataElement::WriteSize() const {
  switch (type_) {
    case Type::kNull:
      return 1;
    case Type::kBoolean:
      return 2;
    case Type::kUnsignedInt:
    case Type::kSignedInt:
    case Type::kUuid:
      return 1 + (1 << static_cast<uint8_t>(size_));
    case Type::kString:
    case Type::kUrl:
      return 1 + (1 << (static_cast<uint8_t>(size_) - 5)) + bytes_.size();
    case Type::kSequence:
    case Type::kAlternative:
      return 1 + (1 << (static_cast<uint8_t>(size_) - 5)) + AggregateSize(aggregate_);
  }
}

size_t DataElement::Write(MutableByteBuffer* buffer) const {
  if (buffer->size() < WriteSize()) {
    bt_log(TRACE, "sdp", "not enough space in buffer (%zu < %zu)", buffer->size(), WriteSize());
    return 0;
  }

  uint8_t type_and_size = static_cast<uint8_t>(type_) | static_cast<uint8_t>(size_);
  buffer->Write(&type_and_size, 1);
  size_t pos = 1;

  MutableBufferView cursor = buffer->mutable_view(pos);

  switch (type_) {
    case Type::kNull: {
      return pos;
    }
    case Type::kBoolean: {
      uint8_t val = int_value_ != 0 ? 1 : 0;
      cursor.Write(&val, sizeof(val));
      pos += 1;
      return pos;
    }
    case Type::kUnsignedInt: {
      if (size_ == Size::kOneByte) {
        uint8_t val = static_cast<uint8_t>(uint_value_);
        cursor.Write(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        pos += sizeof(val);
      } else if (size_ == Size::kTwoBytes) {
        cursor.WriteObj(htobe16(static_cast<uint16_t>(uint_value_)));
        pos += sizeof(uint16_t);
      } else if (size_ == Size::kFourBytes) {
        cursor.WriteObj(htobe32(static_cast<uint32_t>(uint_value_)));
        pos += sizeof(uint32_t);
      } else if (size_ == Size::kEightBytes) {
        uint64_t val = htobe64(uint_value_);
        cursor.Write(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        pos += sizeof(val);
      }
      return pos;
    }
    case Type::kSignedInt: {
      if (size_ == Size::kOneByte) {
        int8_t val = static_cast<int8_t>(int_value_);
        cursor.Write(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        pos += sizeof(val);
      } else if (size_ == Size::kTwoBytes) {
        cursor.WriteObj(htobe16(static_cast<int16_t>(int_value_)));
        pos += sizeof(uint16_t);
      } else if (size_ == Size::kFourBytes) {
        cursor.WriteObj(htobe32(static_cast<int32_t>(int_value_)));
        pos += sizeof(uint32_t);
      } else if (size_ == Size::kEightBytes) {
        int64_t val = htobe64(static_cast<int64_t>(int_value_));
        cursor.Write(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        pos += sizeof(val);
      }
      return pos;
    }
    case Type::kUuid: {
      size_t written = uuid_.ToBytes(&cursor);
      BT_DEBUG_ASSERT(written);
      // SDP is big-endian, so reverse.
      std::reverse(cursor.mutable_data(), cursor.mutable_data() + written);
      pos += written;
      return pos;
    }
    case Type::kString:
    case Type::kUrl: {
      size_t used = WriteLength(&cursor, bytes_.size());
      BT_DEBUG_ASSERT(used);
      pos += used;
      cursor.Write(bytes_.data(), bytes_.size(), used);
      pos += bytes_.size();
      return pos;
    }
    case Type::kSequence:
    case Type::kAlternative: {
      size_t used = WriteLength(&cursor, AggregateSize(aggregate_));
      BT_DEBUG_ASSERT(used);
      pos += used;
      cursor = cursor.mutable_view(used);
      for (const auto& elem : aggregate_) {
        used = elem.Write(&cursor);
        BT_DEBUG_ASSERT(used);
        pos += used;
        cursor = cursor.mutable_view(used);
      }
      return pos;
    }
  }
  return 0;
}

const DataElement* DataElement::At(size_t idx) const {
  if ((type_ != Type::kSequence && type_ != Type::kAlternative) || (idx >= aggregate_.size())) {
    return nullptr;
  }
  return &aggregate_[idx];
}

std::string DataElement::ToString() const {
  switch (type_) {
    case Type::kNull:
      return std::string("Null");
    case Type::kBoolean:
      return bt_lib_cpp_string::StringPrintf("Boolean(%s)", int_value_ ? "true" : "false");
    case Type::kUnsignedInt:
      return bt_lib_cpp_string::StringPrintf("UnsignedInt:%zu(%lu)", WriteSize() - 1, uint_value_);
    case Type::kSignedInt:
      return bt_lib_cpp_string::StringPrintf("SignedInt:%zu(%ld)", WriteSize() - 1, int_value_);
    case Type::kUuid:
      return bt_lib_cpp_string::StringPrintf("UUID(%s)", uuid_.ToString().c_str());
    case Type::kString:
      return bt_lib_cpp_string::StringPrintf("String(%s)",
                                             bytes_.Printable(0, bytes_.size()).c_str());
    case Type::kUrl:
      return bt_lib_cpp_string::StringPrintf("Url(%s)", bytes_.Printable(0, bytes_.size()).c_str());
    case Type::kSequence: {
      std::string str;
      for (const auto& it : aggregate_) {
        str += it.ToString() + " ";
      }
      return bt_lib_cpp_string::StringPrintf("Sequence { %s}", str.c_str());
    }
    case Type::kAlternative: {
      std::string str;
      for (const auto& it : aggregate_) {
        str += it.ToString() + " ";
      }
      return bt_lib_cpp_string::StringPrintf("Alternatives { %s}", str.c_str());
    }
    default:
      bt_log(TRACE, "sdp", "unhandled type (%hhu) in ToString()", type_);
      // Fallthrough to unknown.
  }

  return "(unknown)";
}
}  // namespace bt::sdp
