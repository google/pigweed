// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_PACKET_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt {

// This file defines classes which provide the interface for constructing HCI packets and
// reading/writing them using Emboss (https://github.com/google/emboss).
//
// Emboss does not own memory; it provides structured views into user allocated memory. These views
// are specified in Emboss source files such as hci-protocol.emb, which implements the HCI protocol
// packet definitions.
//
// This file defines two classes: StaticPacket, which provides an Emboss view over a statically
// allocated buffer, and DynamicPacket, which is part of a class hierarchy that provides Emboss
// views over dynamic memory.
//
// EXAMPLE:
//
// Consider the following Emboss definition of the HCI Command packet header and Inquiry Command.
//
//  [(cpp) namespace: "bt::hci_spec"]
//  struct EmbossCommandHeader:
//    0     [+2] OpCodeBits opcode
//    $next [+1] UInt parameter_total_size
//
//  struct InquiryCommand:
//    let hdr_size = EmbossCommandHeader.$size_in_bytes
//    0     [+hdr_size] EmbossCommandHeader header
//    $next [+3] InquiryAccessCode lap
//    $next [+1] UInt inquiry_length
//    $next [+1] UInt num_responses
//
// The Emboss compiler generates two types of view for each struct. In the case of InquiryCommand,
// it generates InquiryCommandView (read-only) and InquiryCommandWriter (read & writable). We can
// parameterize StaticPacket over one of these views to read and/or write an Inquiry packet:
//
//  bt::StaticPacket<bt::hci_spec::InquiryCommandWriter> packet;
//  auto view = packet.view();
//  view.inquiry_length().Write(100);
//  view.lap().Write(bt::hci_spec::InquiryAccessCode::GIAC);
//  cout << "inquiry_length = " << view.inquiry_length().Read();
//
// StaticPacket does not currently support packets with variable length.
template <typename T>
class StaticPacket {
 public:
  // Returns an Emboss view over the buffer. Emboss views consist of two pointers and a length, so
  // they are cheap to construct on-demand.
  T view() {
    T view(buffer_.mutable_data(), buffer_.size());
    BT_ASSERT(view.IsComplete());
    return view;
  }

  BufferView data() const { return BufferView{buffer_, buffer_.size()}; }
  void SetToZeros() { buffer_.SetToZeros(); }

 private:
  // The intrinsic size of an Emboss struct is the size required to hold all of its fields. An
  // Emboss view has a static IntrinsicSizeInBytes() accessor if the struct does not have dynamic
  // length (i.e. not a variable length packet).
  StaticByteBuffer<T::IntrinsicSizeInBytes().Read()> buffer_;
};

// DynamicPacket is the parent class of a two-level class hierarchy that implements
// dynamically-allocated HCI packets to which reading/writing is mediated by Emboss.
//
// DynamicPacket contains data and methods that are universal across packet type. Its children are
// packet type specializations, i.e. Command, Event, ACL, and Sco packets. These classes provide
// header-type-specific functionality.
//
// Instances of DynamicPacket should not be constructed directly. Instead, packet type
// specialization classes should provide static factory functions.
//
// See EmbossCommandPacket in emboss_control_packets.h for an example of a packet type
// specialization.
class DynamicPacket {
 public:
  // Returns an Emboss view over the buffer. Unlike StaticPacket, which ensures type security as a
  // struct parameterized over a particular Emboss view type, DynamicPacket is a generic type for
  // all packets, so view() is to be parameterized over an Emboss view type on each call.
  template <typename T>
  T view() {
    T view(buffer_.mutable_data(), size());
    BT_ASSERT_MSG(view.IsComplete(),
                  "emboss packet buffer not large enough to hold requested view");
    return view;
  }

  // Returns the size of the packet, i.e. payload size + header size.
  size_t size() const { return buffer_.size(); }
  BufferView data() const { return {buffer_.data(), size()}; }
  MutableBufferView mutable_data() { return {buffer_.mutable_data(), size()}; }

 protected:
  // Construct the buffer to hold |packet_size| bytes (payload + header).
  explicit DynamicPacket(size_t packet_size) : buffer_(packet_size) {}

 private:
  DynamicByteBuffer buffer_;
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_PACKET_H_
