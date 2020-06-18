// Copyright 2020 The Pigweed Authors
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
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#include "pw_preprocessor/compiler.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw {

// ByteOrder enum class enables users of ByteBuffer to specify the
// desired Endianness for ordering the values to be inserted.
enum class ByteOrder { kLittleEndian, kBigEndian };

inline constexpr ByteOrder kSystemEndianness =
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ? ByteOrder::kLittleEndian
                                               : ByteOrder::kBigEndian);

// ByteBuilder facilitates building bytes in a fixed-size buffer.
// BytesBuilders never overflow. Status is tracked for each operation and
// an overall status is maintained, which reflects the most recent error.
//
// A ByteBuilder does not own the buffer it writes to. It can be used to write
// bytes to any buffer. The ByteBuffer template class, defined below,
// allocates a buffer alongside a ByteBuilder.
class ByteBuilder {
 public:
  // Creates an empty ByteBuilder.
  constexpr ByteBuilder(span<std::byte> buffer) : buffer_(buffer), size_(0) {}

  // Disallow copy/assign to avoid confusion about where the bytes is actually
  // stored. ByteBuffers may be copied into one another.
  ByteBuilder(const ByteBuilder&) = delete;

  ByteBuilder& operator=(const ByteBuilder&) = delete;

  // Returns the contents of the bytes buffer. Always null-terminated.
  const std::byte* data() const { return buffer_.data(); }

  // Returns the ByteBuilder's status, which reflects the most recent error
  // that occurred while updating the bytes. After an update fails, the status
  // remains non-OK until it is cleared with clear() or clear_status(). Returns:
  //
  //     OK if no errors have occurred
  //     RESOURCE_EXHAUSTED if output to the ByteBuilder was truncated
  //     INVALID_ARGUMENT if printf-style formatting failed
  //     OUT_OF_RANGE if an operation outside the buffer was attempted
  //
  Status status() const { return status_; }

  // Returns status() and size() as a StatusWithSize.
  StatusWithSize status_with_size() const {
    return StatusWithSize(status_, size_);
  }

  // True if status() is Status::OK.
  bool ok() const { return status_.ok(); }

  // True if the bytes builder is empty.
  bool empty() const { return size() == 0u; }

  // Returns the current length of the bytes.
  size_t size() const { return size_; }

  // Returns the maximum length of the bytes.
  size_t max_size() const { return buffer_.size(); }

  // Clears the bytes and resets its error state.
  void clear() {
    size_ = 0;
    status_ = Status::OK;
  };

  // Sets the statuses to Status::OK;
  void clear_status() { status_ = Status::OK; }

  // Appends a single byte. Sets the status to RESOURCE_EXHAUSTED if the
  // byte cannot be added because the buffer is full.
  void push_back(std::byte b) { append(1, b); }

  // Removes the last byte. Sets the status to OUT_OF_RANGE if the buffer
  // is empty (in which case the unsigned overflow is intentional).
  void pop_back() PW_NO_SANITIZE("unsigned-integer-overflow") {
    resize(size() - 1);
  }

  // Appends the provided byte count times.
  ByteBuilder& append(size_t count, std::byte b);

  // Appends count bytes from 'bytes' to the end of the ByteBuilder. If count
  // exceeds the remaining space in the ByteBuffer, no bytes will be appended
  // and the status is set to RESOURCE_EXHAUSTED.
  ByteBuilder& append(const void* bytes, size_t count);

  // Appends bytes from a byte span that calls the pointer/length version.
  ByteBuilder& append(span<std::byte> bytes) {
    return append(bytes.data(), bytes.size());
  }

  // Sets the ByteBuilder's size. This function only truncates; if
  // new_size > size(), it sets status to OUT_OF_RANGE and does nothing.
  void resize(size_t new_size);

  // Put methods for inserting different 8-bit ints
  ByteBuilder& PutUint8(uint8_t val) { return WriteInOrder(val); }

  ByteBuilder& PutInt8(int8_t val) { return WriteInOrder(val); }

  // Put methods for inserting different 16-bit ints
  ByteBuilder& PutUint16(uint16_t value,
                         ByteOrder order = ByteOrder::kLittleEndian) {
    if (kSystemEndianness != order) {
      value = ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
    }
    return WriteInOrder(value);
  }

  ByteBuilder& PutInt16(int16_t value,
                        ByteOrder order = ByteOrder::kLittleEndian) {
    return PutUint16(static_cast<uint16_t>(value), order);
  }

  // Put methods for inserting different 32-bit ints
  ByteBuilder& PutUint32(uint32_t value,
                         ByteOrder order = ByteOrder::kLittleEndian) {
    if (kSystemEndianness != order) {
      value = ((value & 0x000000FF) << 3 * 8) |  //
              ((value & 0x0000FF00) << 1 * 8) |  //
              ((value & 0x00FF0000) >> 1 * 8) |  //
              ((value & 0xFF000000) >> 3 * 8);
    }
    return WriteInOrder(value);
  }

  ByteBuilder& PutInt32(int32_t value,
                        ByteOrder order = ByteOrder::kLittleEndian) {
    return PutUint32(static_cast<uint32_t>(value), order);
  }

  // Put methods for inserting different 64-bit ints
  ByteBuilder& PutUint64(uint64_t value,
                         ByteOrder order = ByteOrder::kLittleEndian) {
    if (kSystemEndianness != order) {
      value = ((value & 0x00000000000000FF) << 7 * 8) |  //
              ((value & 0x000000000000FF00) << 5 * 8) |  //
              ((value & 0x0000000000FF0000) << 3 * 8) |  //
              ((value & 0x00000000FF000000) << 1 * 8) |  //
              ((value & 0x000000FF00000000) >> 1 * 8) |  //
              ((value & 0x0000FF0000000000) >> 3 * 8) |  //
              ((value & 0x00FF000000000000) >> 5 * 8) |  //
              ((value & 0xFF00000000000000) >> 7 * 8);
    }
    return WriteInOrder(value);
  }

  ByteBuilder& PutInt64(int64_t value,
                        ByteOrder order = ByteOrder::kLittleEndian) {
    return PutUint64(static_cast<uint64_t>(value), order);
  }

 protected:
  // Functions to support ByteBuffer copies.
  constexpr ByteBuilder(const span<std::byte>& buffer, const ByteBuilder& other)
      : buffer_(buffer), size_(other.size_), status_(other.status_) {}

  void CopySizeAndStatus(const ByteBuilder& other) {
    size_ = other.size_;
    status_ = other.status_;
  };

 private:
  template <typename T>
  ByteBuilder& WriteInOrder(T value) {
    return append(&value, sizeof(value));
  }
  size_t ResizeForAppend(size_t bytes_to_append);

  const span<std::byte> buffer_;

  size_t size_;
  Status status_;
};

// ByteBuffers declare a buffer along with a ByteBuilder.
template <size_t size_bytes>
class ByteBuffer : public ByteBuilder {
 public:
  ByteBuffer() : ByteBuilder(buffer_) {}

  // ByteBuffers of the same size may be copied and assigned into one another.
  ByteBuffer(const ByteBuffer& other) : ByteBuilder(buffer_, other) {
    CopyContents(other);
  }

  // A smaller ByteBuffer may be copied or assigned into a larger one.
  template <size_t other_size_bytes>
  ByteBuffer(const ByteBuffer<other_size_bytes>& other)
      : ByteBuilder(buffer_, other) {
    static_assert(ByteBuffer<other_size_bytes>::max_size() <= max_size(),
                  "A ByteBuffer cannot be copied into a smaller buffer");
    CopyContents(other);
  }

  template <size_t other_size_bytes>
  ByteBuffer& operator=(const ByteBuffer<other_size_bytes>& other) {
    assign<other_size_bytes>(other);
    return *this;
  }

  ByteBuffer& operator=(const ByteBuffer& other) {
    assign<size_bytes>(other);
    return *this;
  }

  template <size_t other_size_bytes>
  ByteBuffer& assign(const ByteBuffer<other_size_bytes>& other) {
    static_assert(ByteBuffer<other_size_bytes>::max_size() <= max_size(),
                  "A ByteBuffer cannot be copied into a smaller buffer");
    CopySizeAndStatus(other);
    CopyContents(other);
    return *this;
  }

  // Returns the maximum length of the bytes that can be inserted in the bytes
  // buffer.
  static constexpr size_t max_size() { return size_bytes; }

  // Returns a ByteBuffer<size_bytes>& instead of a generic ByteBuilder& for
  // append calls.
  template <typename... Args>
  ByteBuffer& append(Args&&... args) {
    ByteBuilder::append(std::forward<Args>(args)...);
    return *this;
  }

 private:
  template <size_t kOtherSize>
  void CopyContents(const ByteBuffer<kOtherSize>& other) {
    std::memcpy(buffer_.data(), other.data(), other.size());
  }

  std::array<std::byte, size_bytes> buffer_;
};

}  // namespace pw
