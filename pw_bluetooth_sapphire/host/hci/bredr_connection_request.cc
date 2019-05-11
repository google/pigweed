// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_request.h"

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/status.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"

namespace bt {
namespace hci {

std::unique_ptr<CommandPacket> CreateConnectionPacket(
    DeviceAddress address,
    std::optional<PageScanRepetitionMode> page_scan_repetition_mode,
    std::optional<uint16_t> clock_offset) {
  auto request = CommandPacket::New(kCreateConnection,
                                    sizeof(CreateConnectionCommandParams));
  auto params =
      request->mutable_view()->mutable_payload<CreateConnectionCommandParams>();

  params->bd_addr = address.value();
  params->packet_type = htole16(kEnableAllPacketTypes);

  // The Page Scan Repetition Mode of the remote device as retrieved by Inquiry.
  // If we do not have one for the device, opt for R2 so we will send for at
  // least 2.56s
  if (page_scan_repetition_mode)
    params->page_scan_repetition_mode = *page_scan_repetition_mode;
  else
    params->page_scan_repetition_mode =
        PageScanRepetitionMode::kR2;  // Every 2.56 seconds

  params->reserved = 0;  // Reserved, must be set to 0.

  // Clock Offset.  The lower 15 bits are set to the clock offset as retrieved
  // by an Inquiry. The highest bit is set to 1 if the rest of this parameter
  // is valid. If we don't have one, use the default.
  if (clock_offset)
    params->clock_offset = htole16(*clock_offset);
  else
    params->clock_offset = 0x0000;

  params->allow_role_switch = static_cast<uint8_t>(
      RoleSwitchBits::kDisallowRoleSwitch);  // Do not allow role switch

  return request;
}

void BrEdrConnectionRequest::CreateConnection(
    CommandChannel* command_channel, async_dispatcher_t* dispatcher,
    std::optional<uint16_t> clock_offset,
    std::optional<PageScanRepetitionMode> page_scan_repetition_mode,
    zx::duration timeout, OnCompleteDelegate on_command_fail) {
  ZX_DEBUG_ASSERT(timeout > zx::msec(0));

  // HCI Command Status Event will be sent as our completion callback.
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto complete_cb = [self, timeout, peer_id = peer_id_,
                      on_command_fail = std::move(on_command_fail)](
                         auto, const EventPacket& event) {
    ZX_DEBUG_ASSERT(event.event_code() == kCommandStatusEventCode);

    if (!self)
      return;

    Status status = event.ToStatus();
    if (!status) {
      on_command_fail(status, peer_id);
    } else {
      // The request was started but has not completed; initiate the command
      // timeout period. NOTE: The request will complete when the controller
      // asynchronously notifies us of with a BrEdr Connection Complete event.
      self->timeout_task_.PostDelayed(async_get_default_dispatcher(), timeout);
    }
  };

  auto packet = CreateConnectionPacket(peer_address_, page_scan_repetition_mode,
                                       clock_offset);

  command_channel->SendCommand(std::move(packet), dispatcher,
                               std::move(complete_cb), kCommandStatusEventCode);
}

// Status is either a Success or an Error value
Status BrEdrConnectionRequest::CompleteRequest(Status status) {
  bt_log(TRACE, "hci-bredr", "connection complete - status: %s",
         bt_str(status));
  timeout_task_.Cancel();

  if (!status.is_success()) {
    if (state_ == RequestState::kTimedOut) {
      status = Status(HostError::kTimedOut);
    } else if (status.protocol_error() == StatusCode::kUnknownConnectionId) {
      // The "Unknown Connection Identifier" error code is returned if this
      // event was sent due to a successful cancellation via the
      // HCI_Create_Connection_Cancel command
      // See Core Spec v5.0 Vol 2, Part E, Section 7.1.7
      status = Status(HostError::kCanceled);
    }
  }
  return status;
}

void BrEdrConnectionRequest::Timeout() {
  // If the request was cancelled, this handler will have been removed
  ZX_ASSERT(state_ == RequestState::kPending);
  bt_log(INFO, "hci-bredr", "create connection timed out: canceling request");
  state_ = RequestState::kTimedOut;
  timeout_task_.Cancel();
}

bool BrEdrConnectionRequest::Cancel() {
  if (!(state_ == RequestState::kPending)) {
    bt_log(WARN, "hci-bredr", "connection attempt already canceled!");
    return false;
  }
  state_ = RequestState::kCanceled;
  timeout_task_.Cancel();
  return true;
}

BrEdrConnectionRequest::~BrEdrConnectionRequest() { Cancel(); }

}  // namespace hci
}  // namespace bt
