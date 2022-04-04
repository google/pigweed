// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PACKET_VIEW_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PACKET_VIEW_H_

#include <zircon/assert.h>

#include <cstdint>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt {

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
//   } __PACKED;
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
class PacketView {
 public:
  // Initializes this Packet to operate over |buffer|. |payload_size| is the
  // size of the packet payload not including the packet header. A
  // |payload_size| value of 0 indicates that the packet contains no payload.
  explicit PacketView(const ByteBuffer* buffer, size_t payload_size = 0u)
      : buffer_(buffer), size_(sizeof(HeaderType) + payload_size) {
    ZX_ASSERT(buffer_);
    ZX_ASSERT_MSG(buffer_->size() >= size_, "view size %zu exceeds buffer size %zu", size_,
                  buffer_->size());
  }

  BufferView data() const { return buffer_->view(0, size_); }
  BufferView payload_data() const {
    return buffer_->view(sizeof(HeaderType), size_ - sizeof(HeaderType));
  }

  size_t size() const { return size_; }
  size_t payload_size() const {
    ZX_ASSERT(size() >= sizeof(HeaderType));
    return size() - sizeof(HeaderType);
  }

  HeaderType header() const { return buffer_->To<HeaderType>(); }

  template <typename PayloadType>
  const PayloadType& payload() const {
    ZX_ASSERT(sizeof(PayloadType) <= payload_size());
    return *reinterpret_cast<const PayloadType*>(payload_data().data());
  }

  // Adjusts the size of this PacketView to match the given |payload_size|. This
  // is useful when the exact packet size is not known during construction.
  //
  // This performs runtime checks to make sure that the underlying buffer is
  // approriately sized.
  void Resize(size_t payload_size) { this->set_size(sizeof(HeaderType) + payload_size); }

 protected:
  void set_size(size_t size) {
    ZX_ASSERT(buffer_->size() >= size);
    ZX_ASSERT(size >= sizeof(HeaderType));
    size_ = size;
  }

  const ByteBuffer* buffer() const { return buffer_; }

 private:
  const ByteBuffer* buffer_;
  size_t size_;
};

template <typename HeaderType>
class MutablePacketView : public PacketView<HeaderType> {
 public:
  explicit MutablePacketView(MutableByteBuffer* buffer, size_t payload_size = 0u)
      : PacketView<HeaderType>(buffer, payload_size) {}

  MutableBufferView mutable_data() { return mutable_buffer()->mutable_view(0, this->size()); }

  MutableBufferView mutable_payload_data() const {
    return mutable_buffer()->mutable_view(sizeof(HeaderType), this->size() - sizeof(HeaderType));
  }

  uint8_t* mutable_payload_bytes() {
    return this->payload_size() ? mutable_buffer()->mutable_data() + sizeof(HeaderType) : nullptr;
  }

  HeaderType* mutable_header() {
    return reinterpret_cast<HeaderType*>(mutable_buffer()->mutable_data());
  }

  template <typename PayloadType>
  PayloadType* mutable_payload() {
    ZX_ASSERT(sizeof(PayloadType) <= this->payload_size());
    return reinterpret_cast<PayloadType*>(mutable_payload_bytes());
  }

 private:
  MutableByteBuffer* mutable_buffer() const {
    // Cast-away the const. This is OK in this case since we're storing our
    // buffer in the parent class instead of duplicating a non-const version in
    // this class.
    return const_cast<MutableByteBuffer*>(static_cast<const MutableByteBuffer*>(this->buffer()));
  }
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PACKET_VIEW_H_
