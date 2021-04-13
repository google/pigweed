// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_BREDR_CONNECTION_REQUEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_BREDR_CONNECTION_REQUEST_H_

#include <lib/async/default.h>
#include <lib/async/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/status.h"

namespace bt::hci {

// The request can be in three possible states:
enum class RequestState : uint8_t {
  // The connection request is still pending
  kPending,
  // The connection request was intentionally cancelled
  kCanceled,
  // The connection request timed out whilst waiting for a response
  kTimedOut
};

// Bitmask enabling all packets types. By enabling as many as we can, we expect
// the controller to only use the ones it supports
constexpr PacketTypeType kEnableAllPacketTypes =
    static_cast<PacketTypeType>(PacketTypeBits::kEnableDM1) |
    static_cast<PacketTypeType>(PacketTypeBits::kEnableDH1) |
    static_cast<PacketTypeType>(PacketTypeBits::kEnableDM3) |
    static_cast<PacketTypeType>(PacketTypeBits::kEnableDH3) |
    static_cast<PacketTypeType>(PacketTypeBits::kEnableDM5) |
    static_cast<PacketTypeType>(PacketTypeBits::kEnableDH5);

// This class represents a pending request by the BrEdr connector to initiate an
// outgoing connection. It tracks the state of that request and is responsible
// for running a call back when the connection status updates
//
// There should be only one of these at any given time, an it is managed by the
// BrEdrConnectionManager
class BrEdrConnectionRequest final {
 public:
  using OnCompleteDelegate = fit::function<void(Status, PeerId)>;

  BrEdrConnectionRequest(PeerId id, DeviceAddress addr, fit::closure timeout_cb)
      : state_(RequestState::kPending),
        peer_id_(id),
        peer_address_(addr),
        timeout_task_(std::move(timeout_cb)),
        weak_ptr_factory_(this) {}

  ~BrEdrConnectionRequest();

  // Send the CreateConnection command over |command_channel| and begin the
  // create connection procedure. If the command status returns an error, then
  // |on_command_fail| will be scheduled on |dispatcher|. The |clock_offset|
  // and |page_scan_repetition| parameters are standard parameters found in
  // Core Spec 5.0, Vol 2, Part E, section 7.1.5
  // |timeout| is the command timeout; this is how long we give from the point
  // we receive the CommandStatus response from the controller until we cancel
  // the procedure if we have not received ConnectionComplete
  void CreateConnection(CommandChannel* command_channel, async_dispatcher_t* dispatcher,
                        std::optional<uint16_t> clock_offset,
                        std::optional<PageScanRepetitionMode> page_scan_repetition_mode,
                        zx::duration timeout, OnCompleteDelegate on_command_fail);

  PeerId peer_id() const { return peer_id_; }
  DeviceAddress peer_address() const { return peer_address_; }

  // Complete the request, either successfully or not, and return the status
  // of the Request - In the case of Timeout or Cancellation, this will be
  // different from the status sent by the controller.
  Status CompleteRequest(Status status);

  // Mark the request as Timed out; triggered when the command timeout runs out
  // and called by BrEdrConnectionManager;
  void Timeout();

  // Attempt to mark the request as Canceled, and returns true if successful.
  // This is called during cleanup to ensure connection procedures are not
  // orphaned
  bool Cancel();

 private:
  RequestState state_;
  PeerId peer_id_;
  DeviceAddress peer_address_;

  async::TaskClosure timeout_task_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrConnectionRequest> weak_ptr_factory_;
};

}  // namespace bt::hci
#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_BREDR_CONNECTION_REQUEST_H_
