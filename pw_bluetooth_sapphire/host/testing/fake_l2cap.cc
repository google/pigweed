// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_l2cap.h"

#include <endian.h>

namespace bt {
namespace testing {

FakeL2cap::FakeL2cap(UnexpectedPduCallback unexpected_pdu_callback)
    : unexpected_pdu_callback_(std::move(unexpected_pdu_callback)) {}

void FakeL2cap::RegisterHandler(l2cap::ChannelId cid, ChannelReceiveCallback callback) {
  if (callbacks_.find(cid) != callbacks_.end()) {
    bt_log(WARN, "fake-hci", "Overwriting previous handler for Channel ID %hu", cid);
  }
  callbacks_.insert_or_assign(cid, std::move(callback));
}

void FakeL2cap::HandlePdu(hci::ConnectionHandle conn, const ByteBuffer& pdu) {
  if (pdu.size() < sizeof(l2cap::BasicHeader)) {
    bt_log(WARN, "fake-hci", "malformed L2CAP packet!");
    return;
  }

  // Extract channel ID and strip L2CAP header from the pdu.
  const auto& header = pdu.As<l2cap::BasicHeader>();
  l2cap::ChannelId cid = le16toh(header.channel_id);
  auto header_len = sizeof(header);
  auto payload_len = le16toh(header.length);
  auto sdu = DynamicByteBuffer(payload_len);
  pdu.Copy(&sdu, header_len, payload_len);

  // Execute corresponding handler function.
  auto handler = callbacks_.find(cid);
  if (handler == callbacks_.end()) {
    return unexpected_pdu_callback_(conn, pdu);
  } else {
    return handler->second(conn, sdu);
  }
}

}  // namespace testing
}  // namespace bt
