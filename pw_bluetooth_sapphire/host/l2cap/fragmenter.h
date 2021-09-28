// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FRAGMENTER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FRAGMENTER_H_

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/pdu.h"

namespace bt::l2cap {

enum class FrameCheckSequenceOption {
  kNoFcs,      // FCS is not appended to the L2CAP frame
  kIncludeFcs  // FCS is appended to the L2CAP frame
};

// Represents an unfragmented view of a complete L2CAP frame, used to construct PDUs. Unlike PDU,
// this does not own its underlying data and is read-only. To avoid extraneous copies, the only way
// to access the view is to perform a copy from a slice of the view.
class OutboundFrame final {
 public:
  OutboundFrame(ChannelId channel_id, const ByteBuffer& data, FrameCheckSequenceOption fcs_option);

  // Returns the total size of the frame including the L2CAP Basic Header and Information payload.
  [[nodiscard]] size_t size() const;

  // Fills |fragment_payload| with frame data starting at |offset| into the frame, up to the
  // fragment's capacity or the end of this frame, whichever comes first.
  void WriteToFragment(MutableBufferView fragment_payload, size_t offset);

 private:
  using BasicHeaderBuffer = StaticByteBuffer<sizeof(BasicHeader)>;
  using FrameCheckSequenceBuffer = StaticByteBuffer<sizeof(FrameCheckSequence)>;

  bool include_fcs() const { return fcs_option_ == FrameCheckSequenceOption::kIncludeFcs; }

  // Build wire representation of Basic L2CAP header for this frame.
  BasicHeaderBuffer MakeBasicHeader() const;

  // Build wire representation of Frame Check Sequence for this frame.
  // Used to initialize |fcs_|. All other fields must have already been initialized.
  FrameCheckSequenceBuffer MakeFcs() const;

  const ChannelId channel_id_;
  const BufferView data_;
  const FrameCheckSequenceOption fcs_option_;
  const std::optional<FrameCheckSequenceBuffer> fcs_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(OutboundFrame);
};

// A Fragmenter is used to construct L2CAP PDUs composed of fragments that can
// be sent over the HCI ACL data channel. This is intended for building PDUs
// that will be sent in the host-to-controller direction only.
//
// Each instance of Fragmenter is intended to operate on a single logical link.
//
// THREAD-SAFETY:
//
// This class is not thread-safe. External locking should be provided if an
// instance will be accessed on multiple threads.
class Fragmenter final {
 public:
  // |max_acl_payload_size| is the maximum number of bytes that should be
  // allowed in a ACL data packet, exluding the header. |connection_handle|
  // represents the logical link that this Fragmenter operates on.
  //
  // NOTE: |max_acl_payload_size| is required by the spec to be at least 27 (see
  // Core Spec v5.0, Vol 2, Part E, Section 5.4.2). We do not enforce this here
  // as unit tests are allowed to pass a smaller number.
  Fragmenter(hci_spec::ConnectionHandle connection_handle,
             uint16_t max_acl_payload_size = hci_spec::kMaxACLPayloadSize);

  void set_max_acl_payload_size(size_t value) { max_acl_payload_size_ = value; }

  // Constructs and returns a PDU to be sent over the L2CAP channel |channel_id|. |data| will be
  // treated as the Information payload of a B-frame, i.e. the PDU will contain:
  //
  //   <Basic L2CAP header><data>[FCS]
  //
  // All L2CAP frames have a Basic L2CAP header and optionally an FCS footer and can be constructed
  // using this method.
  //
  // If |flushable| is true, then this will build an automatically flushable L2CAP PDU.
  // Automatically flushable packets are sent over ACL-U logical links based on the setting of an
  // automatic flush timer. Only non-automatically flushable PDUs can be sent over LE-U links (see
  // Core Spec v5.0, Vol 2, Part E, Section 5.4.2).
  [[nodiscard]] PDU BuildFrame(ChannelId channel_id, const ByteBuffer& data,
                               FrameCheckSequenceOption fcs_option, bool flushable = false) const;

 private:
  hci_spec::ConnectionHandle connection_handle_;
  size_t max_acl_payload_size_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Fragmenter);
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FRAGMENTER_H_
