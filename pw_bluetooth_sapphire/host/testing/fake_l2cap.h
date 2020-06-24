// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_L2CAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_L2CAP_H_

#include <lib/fit/function.h>

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace testing {

// This class unpacks data units received from ACL-U and LE-U logical links
// into L2CAP SDUs and then routes them to indvidually-registered handler
// functions. Each FakePeer should have its own FakeL2cap instance and its
// own set of ACL-U and LE-U logical links.
class FakeL2cap final {
 public:
  // After registering a channel with RegisterHandler, ChannelReceiveCallback
  // is called with a received L2CAP Service Data Unit |sdu| when FakeL2cap
  // handles a packet for the registered channel. Includes the |handle| that
  // the L2CAP packet was received on.
  using ChannelReceiveCallback =
      fit::function<void(hci::ConnectionHandle handle, const ByteBuffer& sdu)>;

  // When FakeL2cap receives a packet with a ChannelID that does not have a
  // registered handler, it calls UnexpectedPduCallback (set via the
  // constructor or defaulted to a no-op). To aid with debugging, this callback
  // takes the entire Protocol Data Unit |pdu| (including the intact L2CAP
  // header). Also includes the |handle| that the L2CAP packet was received on.
  using UnexpectedPduCallback =
      fit::function<void(hci::ConnectionHandle handle, const ByteBuffer& pdu)>;

  // Calls |unexpected_pdu_callback| for packets received that don't have a
  // handler registered. Defaults to a no-op if no callback provided.
  explicit FakeL2cap(UnexpectedPduCallback unexpected_pdu_callback = [](auto handle, auto& pdu) {});

  // Register |callback| to be called when a Service Data Unit (SDU) is
  // received on an L2CAP channel identified by |cid|. |callback| will be
  // retained until overwritten by another RegisterHandler call or this
  // class is destroyed. To remove a specific |callback|, overwrite it by
  // registering a no-op (or other handler) on the corresponding |cid|.
  void RegisterHandler(l2cap::ChannelId cid, ChannelReceiveCallback callback);

  // Routes the |pdu| to the appropriate calllback function by extracting the
  // ChannelID of the received packet |pdu| and calling the corresponding
  // registered handler function (and providing it with the |handle| the packet
  // was received on and the payload Service Data Unit |sdu|.
  void HandlePdu(hci::ConnectionHandle conn, const ByteBuffer& pdu);

 private:
  // Map of channel IDs and corresponding functions.
  std::unordered_map<l2cap::ChannelId, ChannelReceiveCallback> callbacks_;

  // Handler function associated with unexpected PDUs. Defaults to a no-op.
  UnexpectedPduCallback unexpected_pdu_callback_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeL2cap);
};

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_L2CAP_H_
