// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connector.h"

#include <endian.h>
#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

LowEnergyConnector::PendingRequest::PendingRequest(const DeviceAddress& peer_address,
                                                   StatusCallback status_callback)
    : peer_address(peer_address), status_callback(std::move(status_callback)) {}

LowEnergyConnector::LowEnergyConnector(fxl::WeakPtr<Transport> hci,
                                       LocalAddressDelegate* local_addr_delegate,
                                       async_dispatcher_t* dispatcher,
                                       IncomingConnectionDelegate delegate)
    : dispatcher_(dispatcher),
      hci_(hci),
      local_addr_delegate_(local_addr_delegate),
      delegate_(std::move(delegate)),
      weak_ptr_factory_(this) {
  BT_DEBUG_ASSERT(dispatcher_);
  BT_DEBUG_ASSERT(hci_);
  BT_DEBUG_ASSERT(local_addr_delegate_);
  BT_DEBUG_ASSERT(delegate_);

  auto self = weak_ptr_factory_.GetWeakPtr();
  event_handler_id_ = hci_->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLEConnectionCompleteSubeventCode, [self](const auto& event) {
        if (self) {
          return self->OnConnectionCompleteEvent(event);
        }
        return CommandChannel::EventCallbackResult::kRemove;
      });
}

LowEnergyConnector::~LowEnergyConnector() {
  if (hci_->command_channel()) {
    hci_->command_channel()->RemoveEventHandler(event_handler_id_);
  }
  if (request_pending())
    Cancel();
}

bool LowEnergyConnector::CreateConnection(
    bool use_accept_list, const DeviceAddress& peer_address, uint16_t scan_interval,
    uint16_t scan_window, const hci_spec::LEPreferredConnectionParameters& initial_parameters,
    StatusCallback status_callback, zx::duration timeout) {
  BT_DEBUG_ASSERT(status_callback);
  BT_DEBUG_ASSERT(timeout.get() > 0);

  if (request_pending())
    return false;

  BT_DEBUG_ASSERT(!request_timeout_task_.is_pending());
  pending_request_ = PendingRequest(peer_address, std::move(status_callback));

  local_addr_delegate_->EnsureLocalAddress(
      [this, use_accept_list, peer_address, scan_interval, scan_window, initial_parameters,
       callback = std::move(status_callback), timeout](const auto& address) mutable {
        // Use the identity address if privacy override was enabled.
        CreateConnectionInternal(
            use_local_identity_address_ ? local_addr_delegate_->identity_address() : address,
            use_accept_list, peer_address, scan_interval, scan_window, initial_parameters,
            std::move(callback), timeout);
      });

  return true;
}

void LowEnergyConnector::CreateConnectionInternal(
    const DeviceAddress& local_address, bool use_accept_list, const DeviceAddress& peer_address,
    uint16_t scan_interval, uint16_t scan_window,
    const hci_spec::LEPreferredConnectionParameters& initial_parameters,
    StatusCallback status_callback, zx::duration timeout) {
  // Check if the connection request was canceled via Cancel().
  if (!pending_request_ || pending_request_->canceled) {
    bt_log(DEBUG, "hci-le", "connection request was canceled while obtaining local address");
    pending_request_.reset();
    return;
  }

  BT_DEBUG_ASSERT(!pending_request_->initiating);

  pending_request_->initiating = true;
  pending_request_->local_address = local_address;

  auto request = CommandPacket::New(hci_spec::kLECreateConnection,
                                    sizeof(hci_spec::LECreateConnectionCommandParams));
  auto params = request->mutable_payload<hci_spec::LECreateConnectionCommandParams>();
  params->scan_interval = htole16(scan_interval);
  params->scan_window = htole16(scan_window);
  params->initiator_filter_policy = use_accept_list ? hci_spec::GenericEnableParam::ENABLE
                                                    : hci_spec::GenericEnableParam::DISABLE;

  // TODO(armansito): Use the resolved address types for <5.0 LE Privacy.
  params->peer_address_type =
      peer_address.IsPublic() ? hci_spec::LEAddressType::kPublic : hci_spec::LEAddressType::kRandom;
  params->peer_address = peer_address.value();

  params->own_address_type = local_address.IsPublic() ? hci_spec::LEOwnAddressType::kPublic
                                                      : hci_spec::LEOwnAddressType::kRandom;

  params->conn_interval_min = htole16(initial_parameters.min_interval());
  params->conn_interval_max = htole16(initial_parameters.max_interval());
  params->conn_latency = htole16(initial_parameters.max_latency());
  params->supervision_timeout = htole16(initial_parameters.supervision_timeout());
  params->minimum_ce_length = 0x0000;
  params->maximum_ce_length = 0x0000;

  // HCI Command Status Event will be sent as our completion callback.
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto complete_cb = [self, timeout](auto id, const EventPacket& event) {
    BT_DEBUG_ASSERT(event.event_code() == hci_spec::kCommandStatusEventCode);

    if (!self)
      return;

    Result<> result = event.ToResult();
    if (result.is_error()) {
      self->OnCreateConnectionComplete(result, nullptr);
      return;
    }

    // The request was started but has not completed; initiate the command
    // timeout period. NOTE: The request will complete when the controller
    // asynchronously notifies us of with a LE Connection Complete event.
    self->request_timeout_task_.Cancel();
    self->request_timeout_task_.PostDelayed(async_get_default_dispatcher(), timeout);
  };

  hci_->command_channel()->SendCommand(std::move(request), complete_cb,
                                       hci_spec::kCommandStatusEventCode);
}

void LowEnergyConnector::Cancel() { CancelInternal(false); }

void LowEnergyConnector::CancelInternal(bool timed_out) {
  BT_DEBUG_ASSERT(request_pending());

  if (pending_request_->canceled) {
    bt_log(WARN, "hci-le", "connection attempt already canceled!");
    return;
  }

  // At this point we do not know whether the pending connection request has
  // completed or not (it may have completed in the controller but that does not
  // mean that we have processed the corresponding LE Connection Complete
  // event). Below we mark the request as canceled and tell the controller to
  // cancel its pending connection attempt.
  pending_request_->canceled = true;
  pending_request_->timed_out = timed_out;

  request_timeout_task_.Cancel();

  // Tell the controller to cancel the connection initiation attempt if a
  // request is outstanding. Otherwise there is no need to talk to the
  // controller.
  if (pending_request_->initiating) {
    bt_log(DEBUG, "hci-le", "telling controller to cancel LE connection attempt");
    auto complete_cb = [](auto id, const EventPacket& event) {
      hci_is_error(event, WARN, "hci-le", "failed to cancel connection request");
    };
    auto cancel = CommandPacket::New(hci_spec::kLECreateConnectionCancel);
    hci_->command_channel()->SendCommand(std::move(cancel), complete_cb);

    // A connection complete event will be generated by the controller after processing the cancel
    // command.
    return;
  }

  bt_log(DEBUG, "hci-le", "connection initiation aborted");
  OnCreateConnectionComplete(ToResult(HostError::kCanceled), nullptr);
}

CommandChannel::EventCallbackResult LowEnergyConnector::OnConnectionCompleteEvent(
    const EventPacket& event) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_DEBUG_ASSERT(event.params<hci_spec::LEMetaEventParams>().subevent_code ==
                  hci_spec::kLEConnectionCompleteSubeventCode);

  auto params = event.subevent_params<hci_spec::LEConnectionCompleteSubeventParams>();
  BT_ASSERT(params);

  // First check if this event is related to the currently pending request.
  const bool matches_pending_request =
      pending_request_ && (pending_request_->peer_address.value() == params->peer_address);

  if (Result<> result = event.ToResult(); result.is_error()) {
    if (matches_pending_request) {
      // The "Unknown Connect Identifier" error code is returned if this event
      // was sent due to a successful cancelation via the
      // HCI_LE_Create_Connection_Cancel command (sent by Cancel()).
      if (pending_request_->timed_out) {
        result = ToResult(HostError::kTimedOut);
      } else if (params->status == hci_spec::StatusCode::UNKNOWN_CONNECTION_ID) {
        result = ToResult(HostError::kCanceled);
      }
      OnCreateConnectionComplete(result, nullptr);
    } else {
      bt_log(WARN, "hci-le", "unexpected connection complete event with error received: %s",
             bt_str(result));
    }
    return CommandChannel::EventCallbackResult::kContinue;
  }

  hci_spec::ConnectionHandle handle = le16toh(params->connection_handle);
  DeviceAddress peer_address(AddressTypeFromHCI(params->peer_address_type), params->peer_address);
  hci_spec::LEConnectionParameters connection_params(le16toh(params->conn_interval),
                                                     le16toh(params->conn_latency),
                                                     le16toh(params->supervision_timeout));

  // If the connection did not match a pending request then we pass the
  // information down to the incoming connection delegate.
  if (!matches_pending_request) {
    delegate_(handle, params->role, peer_address, connection_params);
    return CommandChannel::EventCallbackResult::kContinue;
  }

  // A new link layer connection was created. Create an object to track this
  // connection. Destroying this object will disconnect the link.
  auto connection = std::make_unique<LowEnergyConnection>(
      handle, pending_request_->local_address, peer_address, connection_params, params->role, hci_);

  Result<> result = fit::ok();
  if (pending_request_->timed_out) {
    result = ToResult(HostError::kTimedOut);
  } else if (pending_request_->canceled) {
    result = ToResult(HostError::kCanceled);
  }

  // If we were requested to cancel the connection after the logical link
  // is created we disconnect it.
  if (result.is_error()) {
    connection = nullptr;
  }

  OnCreateConnectionComplete(result, std::move(connection));
  return CommandChannel::EventCallbackResult::kContinue;
}

void LowEnergyConnector::OnCreateConnectionComplete(Result<> result,
                                                    std::unique_ptr<LowEnergyConnection> link) {
  BT_DEBUG_ASSERT(pending_request_);

  bt_log(DEBUG, "hci-le", "connection complete - status: %s", bt_str(result));

  request_timeout_task_.Cancel();

  auto status_cb = std::move(pending_request_->status_callback);
  pending_request_.reset();

  status_cb(result, std::move(link));
}

void LowEnergyConnector::OnCreateConnectionTimeout() {
  BT_DEBUG_ASSERT(pending_request_);
  bt_log(INFO, "hci-le", "create connection timed out: canceling request");

  // TODO(armansito): This should cancel the connection attempt only if the connection attempt isn't
  // using the filter accept list.
  CancelInternal(true);
}

}  // namespace bt::hci
