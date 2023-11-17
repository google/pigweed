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

#pragma once
#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

namespace bt {

// Non-templated base class for PacketView to reduce per-instantiation code size
// overhead. This could also be instantiated on <size_t HeaderSize> instead of
// storing a |header_size_| field, which would instantiate one class per header
// size needed for an insignificant time and stack win.
//
// MutablePacketView methods are included in this class instead of a separate
// class to avoid a diamond inheritance hierarchy.
class PacketViewBase {
 public:
  BufferView data() const { return buffer_->view(0, size_); }
  BufferView payload_data() const {
    return buffer_->view(header_size(), size_ - header_size());
  }

  size_t size() const { return size_; }
  size_t payload_size() const {
    BT_ASSERT(size() >= header_size());
    return size() - header_size();
  }

  template <typename PayloadType>
  const PayloadType& payload() const {
    BT_ASSERT(sizeof(PayloadType) <= payload_size());
    return *reinterpret_cast<const PayloadType*>(payload_data().data());
  }

  // Adjusts the size of this PacketView to match the given |payload_size|. This
  // is useful when the exact packet size is not known during construction.
  //
  // This performs runtime checks to make sure that the underlying buffer is
  // appropriately sized.
  void Resize(size_t payload_size) {
    this->set_size(header_size() + payload_size);
  }

 protected:
  PacketViewBase(size_t header_size,
                 const ByteBuffer* buffer,
                 size_t payload_size)
      : header_size_(header_size),
        buffer_(buffer),
        size_(header_size_ + payload_size) {
    BT_ASSERT(buffer_);
    BT_ASSERT_MSG(buffer_->size() >= size_,
                  "view size %zu exceeds buffer size %zu",
                  size_,
                  buffer_->size());
  }

  // Default copy ctor is required for PacketView and MutablePacketView to be
  // copy-constructed, but it should stay protected to avoid upcasting from
  // causing issues.
  PacketViewBase(const PacketViewBase&) = default;

  // Assignment disabled because PacketViewBase doesn't know whether |this| and
  // the assigned parameter are the same type of PacketView<…>.
  PacketViewBase& operator=(const PacketViewBase&) = delete;

  void set_size(size_t size) {
    BT_ASSERT(buffer_->size() >= size);
    BT_ASSERT(size >= header_size());
    size_ = size;
  }

  size_t header_size() const { return header_size_; }

  const ByteBuffer* buffer() const { return buffer_; }

  // Method for MutableBufferView only
  MutableBufferView mutable_data() const {
    return mutable_buffer()->mutable_view(0, this->size());
  }

  // Method for MutableBufferView only
  MutableBufferView mutable_payload_data() const {
    return mutable_buffer()->mutable_view(header_size(),
                                          this->size() - header_size());
  }

  // Method for MutableBufferView only
  uint8_t* mutable_payload_bytes() const {
    return this->payload_size()
               ? mutable_buffer()->mutable_data() + header_size()
               : nullptr;
  }

 private:
  MutableByteBuffer* mutable_buffer() const {
    // For use only by MutableBufferView, which is constructed with a
    // MutableBufferView*. This restores the mutability that is implicitly
    // upcasted away when stored in this Base class.
    return const_cast<MutableByteBuffer*>(
        static_cast<const MutableByteBuffer*>(this->buffer()));
  }

  const size_t header_size_;
  const ByteBuffer* const buffer_;
  size_t size_;
};

// Base class-template for generic packets that contain a header and a payload.
// A PacketView is a light-weight object that operates over a previously
// allocated ByteBuffer without taking ownership of it. The PacketView
// class-template provides a read-only view over the underlying buffer while
// MutablePacketView allows modification of the underlying buffer.
//
// Example usage:
//
//   // Allocate a buffer
//   StaticByteBuffer<512> buffer;
//
//   // Receive some data on the buffer.
//   foo::WriteMyPacket(buffer.mutable_data(), ...);
//
//   // Read packet header contents:
//   struct MyHeaderType {
//     uint8_t field0;
//   };
//
//   PacketView<MyHeaderType> packet(&buffer, 0);
//   std::cout << "My header field is: " << packet.header().field0;
//
//   // If the packet has an expected payload size, pass that into the
//   // constructor:
//   struct MyPayloadType {
//     uint8_t byte_field;
//     uint16_t uint16_field;
//     uint8_t array_field[];
//   } __attribute__((packed));
//
//   MutablePacketView<MyHeaderType> packet(&buffer, sizeof(MyPayloadType) + 2);
//   packet.mutable_payload<MyPayloadType>().byte_field = 0xFF;
//   packet.mutable_payload<MyPayloadType>().uint16_field = 0xFFFF;
//   packet.mutable_payload<MyPayloadType>().array_field[0] = 0x00;
//   packet.mutable_payload<MyPayloadType>().array_field[1] = 0x01;
//
// MutablePacketView allows itself to be resized at any time. This is useful
// when the complete packet payload is unknown prior to reading the header
// contents. For example:
//
//   MutablePacketView<MyHeaderType view(&buffer, my_max_payload_length);
//   view.mutable_data().Write(data);
//   view.Resize(view.header().payload_size);
template <typename HeaderType>
class PacketView : public PacketViewBase {
 public:
  // Initializes this Packet to operate over |buffer|. |payload_size| is the
  // size of the packet payload not including the packet header. A
  // |payload_size| value of 0 indicates that the packet contains no payload.
  explicit PacketView(const ByteBuffer* buffer, size_t payload_size = 0u)
      : PacketViewBase(sizeof(HeaderType), buffer, payload_size) {}

  HeaderType header() const { return buffer()->template To<HeaderType>(); }
};

template <typename HeaderType>
class MutablePacketView : public PacketView<HeaderType> {
 public:
  explicit MutablePacketView(MutableByteBuffer* buffer,
                             size_t payload_size = 0u)
      : PacketView<HeaderType>(buffer, payload_size) {}

  using PacketViewBase::mutable_data;
  using PacketViewBase::mutable_payload_bytes;
  using PacketViewBase::mutable_payload_data;

  HeaderType* mutable_header() const {
    return reinterpret_cast<HeaderType*>(mutable_data().mutable_data());
  }

  template <typename PayloadType>
  PayloadType* mutable_payload() const {
    BT_ASSERT(sizeof(PayloadType) <= this->payload_size());
    return reinterpret_cast<PayloadType*>(mutable_payload_bytes());
  }
};

}  // namespace bt
