// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "byte_buffer.h"

#include "src/connectivity/bluetooth/lib/cpp-string/utf_codecs.h"

namespace bt {

void ByteBuffer::Copy(MutableByteBuffer* out_buffer) const {
  BT_ASSERT(out_buffer);
  CopyRaw(out_buffer->mutable_data(), out_buffer->size(), 0, size());
}

void ByteBuffer::Copy(MutableByteBuffer* out_buffer, size_t pos, size_t size) const {
  BT_ASSERT(out_buffer);
  CopyRaw(out_buffer->mutable_data(), out_buffer->size(), pos, size);
}

std::string ByteBuffer::Printable(size_t pos, size_t size) const {
  BT_ASSERT(pos + size <= this->size());
  const char* region_start = reinterpret_cast<const char*>(data() + pos);
  std::string_view view(region_start, size);

  // If the region already contains only valid UTF-8 characters, it's already printable
  if (bt_lib_cpp_string::IsStringUTF8(view)) {
    return std::string(view);
  }

  std::string ret(size, '\0');
  for (size_t i = 0; i < size; i++) {
    if (std::isprint(view[i])) {
      ret[i] = view[i];
    } else {
      ret[i] = '.';
    }
  }

  return ret;
}

BufferView ByteBuffer::view(size_t pos, size_t size) const {
  BT_ASSERT_MSG(pos <= this->size(), "offset past buffer (pos: %zu, size: %zu)", pos, this->size());
  return BufferView(data() + pos, std::min(size, this->size() - pos));
}

pw::span<const std::byte> ByteBuffer::subspan(size_t pos, size_t size) const {
  BT_ASSERT_MSG(pos <= this->size(), "offset past buffer (pos: %zu, size: %zu)", pos, this->size());
  return pw::span(reinterpret_cast<const std::byte*>(data()) + pos,
                  std::min(size, this->size() - pos));
}

std::string_view ByteBuffer::AsString() const {
  return std::string_view(reinterpret_cast<const char*>(data()), size());
}

std::string ByteBuffer::ToString() const { return std::string(AsString()); }

std::vector<uint8_t> ByteBuffer::ToVector() const {
  std::vector<uint8_t> vec(size());
  MutableBufferView vec_view(vec.data(), vec.size());
  vec_view.Write(*this);
  return vec;
}

void ByteBuffer::CopyRaw(void* dst_data, size_t dst_capacity, size_t src_offset,
                         size_t copy_size) const {
  BT_ASSERT_MSG(copy_size == 0 || dst_data != nullptr, "%zu byte write to pointer %p", copy_size,
                dst_data);
  BT_ASSERT_MSG(copy_size <= dst_capacity,
                "destination not large enough (required: %zu, available: %zu)", copy_size,
                dst_capacity);
  BT_ASSERT_MSG(src_offset <= this->size(),
                "offset exceeds source range (begin: %zu, copy_size: %zu)", src_offset,
                this->size());
  BT_ASSERT_MSG(std::numeric_limits<size_t>::max() - copy_size >= src_offset,
                "end of source range overflows size_t (src_offset: %zu, copy_size: %zu)",
                src_offset, copy_size);
  BT_ASSERT_MSG(src_offset + copy_size <= this->size(),
                "end exceeds source range (end: %zu, copy_size: %zu)", src_offset + copy_size,
                this->size());

  // Data pointers for zero-length buffers are nullptr, over which memcpy has undefined behavior,
  // even for count = 0. Skip the memcpy invocation in that case.
  if (copy_size == 0) {
    return;
  }
  std::memcpy(dst_data, data() + src_offset, copy_size);
}

void MutableByteBuffer::Write(const uint8_t* data, size_t size, size_t pos) {
  BufferView from(data, size);
  MutableBufferView to = mutable_view(pos);
  from.Copy(&to);
}

MutableBufferView MutableByteBuffer::mutable_view(size_t pos, size_t size) {
  BT_ASSERT_MSG(pos <= this->size(), "offset past buffer (pos: %zu, size: %zu)", pos, this->size());
  return MutableBufferView(mutable_data() + pos, std::min(size, this->size() - pos));
}

pw::span<std::byte> MutableByteBuffer::mutable_subspan(size_t pos, size_t size) {
  BT_ASSERT_MSG(pos <= this->size(), "offset past buffer (pos: %zu, size: %zu)", pos, this->size());
  return pw::span(reinterpret_cast<std::byte*>(mutable_data()) + pos,
                  std::min(size, this->size() - pos));
}

DynamicByteBuffer::DynamicByteBuffer() = default;

DynamicByteBuffer::DynamicByteBuffer(size_t buffer_size) : buffer_size_(buffer_size) {
  if (buffer_size == 0) {
    return;
  }

  // make_unique value-initializes the buffer to 0.
  buffer_ = std::make_unique<uint8_t[]>(buffer_size);

  // TODO(armansito): For now this is dumb but we should properly handle the
  // case when we're out of memory.
  BT_ASSERT_MSG(buffer_.get(), "failed to allocate buffer");
}

DynamicByteBuffer::DynamicByteBuffer(const ByteBuffer& buffer)
    : buffer_size_(buffer.size()),
      buffer_(buffer.size() ? std::make_unique<uint8_t[]>(buffer.size()) : nullptr) {
  BT_ASSERT_MSG(!buffer_size_ || buffer_.get(),
                "|buffer| cannot be nullptr when |buffer_size| is non-zero");
  buffer.Copy(this);
}

DynamicByteBuffer::DynamicByteBuffer(const DynamicByteBuffer& buffer)
    : DynamicByteBuffer(static_cast<const ByteBuffer&>(buffer)) {}

DynamicByteBuffer::DynamicByteBuffer(const std::string& buffer) {
  buffer_size_ = buffer.length();
  buffer_ = std::make_unique<uint8_t[]>(buffer_size_);
  memcpy(buffer_.get(), buffer.data(), buffer_size_);
}

DynamicByteBuffer::DynamicByteBuffer(size_t buffer_size, std::unique_ptr<uint8_t[]> buffer)
    : buffer_size_(buffer_size), buffer_(std::move(buffer)) {
  BT_ASSERT_MSG(!buffer_size_ || buffer_.get(),
                "|buffer| cannot be nullptr when |buffer_size| is non-zero");
}

DynamicByteBuffer::DynamicByteBuffer(DynamicByteBuffer&& other) {
  buffer_size_ = other.buffer_size_;
  other.buffer_size_ = 0u;
  buffer_ = std::move(other.buffer_);
}

DynamicByteBuffer& DynamicByteBuffer::operator=(DynamicByteBuffer&& other) {
  buffer_size_ = other.buffer_size_;
  other.buffer_size_ = 0u;
  buffer_ = std::move(other.buffer_);
  return *this;
}

const uint8_t* DynamicByteBuffer::data() const { return buffer_.get(); }

uint8_t* DynamicByteBuffer::mutable_data() { return buffer_.get(); }

size_t DynamicByteBuffer::size() const { return buffer_size_; }

void DynamicByteBuffer::Fill(uint8_t value) { memset(buffer_.get(), value, buffer_size_); }

ByteBuffer::const_iterator DynamicByteBuffer::cbegin() const { return buffer_.get(); }

ByteBuffer::const_iterator DynamicByteBuffer::cend() const { return buffer_.get() + buffer_size_; }

BufferView::BufferView(const ByteBuffer& buffer, size_t size) { *this = buffer.view(0u, size); }

BufferView::BufferView(std::string_view string) {
  size_ = string.size();
  bytes_ = reinterpret_cast<const uint8_t*>(string.data());
}

BufferView::BufferView(const std::vector<uint8_t>& vec) : BufferView(vec.data(), vec.size()) {}

BufferView::BufferView(pw::span<const std::byte> bytes) : BufferView(bytes.data(), bytes.size()) {}

BufferView::BufferView(const void* bytes, size_t size)
    : size_(size), bytes_(static_cast<const uint8_t*>(bytes)) {
  // If |size| non-zero then |bytes| cannot be nullptr.
  BT_ASSERT_MSG(!size_ || bytes_, "|bytes_| cannot be nullptr if |size_| > 0");
}

BufferView::BufferView() = default;

const uint8_t* BufferView::data() const { return bytes_; }

size_t BufferView::size() const { return size_; }

ByteBuffer::const_iterator BufferView::cbegin() const { return bytes_; }

ByteBuffer::const_iterator BufferView::cend() const { return bytes_ + size_; }

MutableBufferView::MutableBufferView(MutableByteBuffer* buffer) {
  BT_ASSERT(buffer);
  size_ = buffer->size();
  bytes_ = buffer->mutable_data();
}

MutableBufferView::MutableBufferView(void* bytes, size_t size)
    : size_(size), bytes_(static_cast<uint8_t*>(bytes)) {
  // If |size| non-zero then |bytes| cannot be nullptr.
  BT_ASSERT_MSG(!size_ || bytes_, "|bytes_| cannot be nullptr if |size_| > 0");
}

MutableBufferView::MutableBufferView() = default;

const uint8_t* MutableBufferView::data() const { return bytes_; }

size_t MutableBufferView::size() const { return size_; }

ByteBuffer::const_iterator MutableBufferView::cbegin() const { return bytes_; }

ByteBuffer::const_iterator MutableBufferView::cend() const { return bytes_ + size_; }

uint8_t* MutableBufferView::mutable_data() { return bytes_; }

void MutableBufferView::Fill(uint8_t value) { memset(bytes_, value, size_); }

}  // namespace bt
