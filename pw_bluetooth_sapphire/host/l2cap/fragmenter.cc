// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fragmenter.h"

#include <endian.h>
#include <zircon/assert.h>

#include <limits>

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace l2cap {
namespace {

// ByteBuffer::Copy does not allow copying to a smaller destination for safety. This clamps the copy
// size to both the source size and the destination size.
size_t CopyBounded(MutableBufferView destination, const ByteBuffer& source) {
  const size_t size = std::min(destination.size(), source.size());
  return source.Copy(&destination, 0, size);
}

}  // namespace

OutboundFrame::OutboundFrame(ChannelId channel_id, const ByteBuffer& data)
    : channel_id_(channel_id), data_(data.view()) {}

// TODO(1511): Account for FCS footer
size_t OutboundFrame::size() const { return sizeof(BasicHeader) + data_.size(); }

void OutboundFrame::WriteToFragment(MutableBufferView fragment_payload, size_t offset) {
  ZX_ASSERT(offset <= size());
  size_t output_offset = 0;
  if (offset < sizeof(BasicHeader)) {
    // Length is "the length of the entire L2CAP PDU in octets, excluding the Length and CID field"
    // (v5.0 Vol 3, Part A, Section 3.3.1)
    const size_t pdu_content_length = size() - sizeof(BasicHeader);
    ZX_ASSERT_MSG(pdu_content_length <= std::numeric_limits<decltype(BasicHeader::length)>::max(),
                  "PDU payload is too large to be encoded");
    BasicHeader header = {};
    header.length = htole16(pdu_content_length);
    header.channel_id = htole16(channel_id_);
    const BufferView header_buffer(&header, sizeof(header));
    output_offset = CopyBounded(fragment_payload, header_buffer.view(offset));
    offset += output_offset;
  }

  // TODO(1511): Check that offset isn't within FCS footer
  output_offset += CopyBounded(fragment_payload.mutable_view(output_offset),
                               data_.view(offset - sizeof(BasicHeader)));
  ZX_ASSERT(output_offset <= fragment_payload.size());
}

Fragmenter::Fragmenter(hci::ConnectionHandle connection_handle, uint16_t max_acl_payload_size)
    : connection_handle_(connection_handle), max_acl_payload_size_(max_acl_payload_size) {
  ZX_ASSERT(connection_handle_);
  ZX_ASSERT(connection_handle_ <= hci::kConnectionHandleMax);
  ZX_ASSERT(max_acl_payload_size_);
  ZX_ASSERT(max_acl_payload_size_ >= sizeof(BasicHeader));
}

// NOTE(armansito): The following method copies the contents of |data| into ACL
// data packets. This copying is currently necessary because the complete HCI
// frame (ACL header + payload fragment) we send over the channel to the bt-hci
// driver need to be stored contiguously before the call to zx_channel_write.
// Plus, we perform the HCI flow-control on the host-stack side which requires
// ACL packets to be buffered.
//
// As our future driver architecture will remove the IPC between the HCI driver
// and the host stack, our new interface could support scatter-gather for the
// header and the payload. Then, the bt-hci driver could read the payload
// fragment directly out of |data| and we would only construct the headers,
// removing the extra copy.
//
// * Current theoretical number of data copies:
//     1. service -> L2CAP channel
//     2. channel -> fragmenter ->(move) HCI layer
//     3. HCI layer ->(zx_channel_write)
//     4. (zx_channel_read)-> bt-hci driver
//     5. bt-hci driver -> transport driver
//
// * Potential number of data copies
//     1. service -> L2CAP channel
//     2. channel -> fragmenter ->(move) HCI layer ->(move) bt-hci driver
//     if buffering is needed:
//       3. bt-hci driver -> transport driver
PDU Fragmenter::BuildBasicFrame(ChannelId channel_id, const ByteBuffer& data,
                                bool flushable) const {
  ZX_DEBUG_ASSERT(data.size() <= kMaxBasicFramePayloadSize);
  ZX_DEBUG_ASSERT(channel_id);

  OutboundFrame frame(channel_id, data);
  const size_t frame_size = frame.size();
  const size_t num_fragments =
      frame_size / max_acl_payload_size_ + (frame_size % max_acl_payload_size_ ? 1 : 0);

  PDU pdu;
  size_t processed = 0;
  for (size_t i = 0; i < num_fragments; i++) {
    ZX_DEBUG_ASSERT(frame_size > processed);

    const size_t fragment_size =
        std::min(frame_size - processed, static_cast<size_t>(max_acl_payload_size_));
    auto pbf = (i ? hci::ACLPacketBoundaryFlag::kContinuingFragment
                  : (flushable ? hci::ACLPacketBoundaryFlag::kFirstFlushable
                               : hci::ACLPacketBoundaryFlag::kFirstNonFlushable));

    // TODO(armansito): allow passing Active Slave Broadcast flag when we
    // support it.
    auto acl_packet = hci::ACLDataPacket::New(connection_handle_, pbf,
                                              hci::ACLBroadcastFlag::kPointToPoint, fragment_size);
    ZX_DEBUG_ASSERT(acl_packet);

    frame.WriteToFragment(acl_packet->mutable_view()->mutable_payload_data(), processed);
    processed += fragment_size;

    pdu.AppendFragment(std::move(acl_packet));
  }

  // The PDU should have been completely processed if we got here.
  ZX_DEBUG_ASSERT(processed == frame_size);

  return pdu;
}

}  // namespace l2cap
}  // namespace bt
