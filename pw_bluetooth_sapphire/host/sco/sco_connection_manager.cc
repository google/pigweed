// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection_manager.h"

#include "lib/async/default.h"

namespace bt::sco {
namespace {

hci::SynchronousConnectionParameters ConnectionParametersToLe(
    hci::SynchronousConnectionParameters params) {
  params.transmit_bandwidth = htole32(params.transmit_bandwidth);
  params.receive_bandwidth = htole32(params.receive_bandwidth);
  params.transmit_coding_format.company_id = htole16(params.transmit_coding_format.company_id);
  params.transmit_coding_format.vendor_codec_id =
      htole16(params.transmit_coding_format.vendor_codec_id);
  params.receive_coding_format.company_id = htole16(params.receive_coding_format.company_id);
  params.receive_coding_format.vendor_codec_id =
      htole16(params.receive_coding_format.vendor_codec_id);
  params.transmit_codec_frame_size_bytes = htole16(params.transmit_codec_frame_size_bytes);
  params.receive_codec_frame_size_bytes = htole16(params.receive_codec_frame_size_bytes);
  params.input_bandwidth = htole32(params.input_bandwidth);
  params.output_bandwidth = htole32(params.output_bandwidth);
  params.input_coding_format.company_id = htole16(params.input_coding_format.company_id);
  params.input_coding_format.vendor_codec_id = htole16(params.input_coding_format.vendor_codec_id);
  params.output_coding_format.company_id = htole16(params.output_coding_format.company_id);
  params.output_coding_format.vendor_codec_id =
      htole16(params.output_coding_format.vendor_codec_id);
  params.max_latency_ms = htole16(params.max_latency_ms);
  params.packet_types = htole16(params.packet_types);
  return params;
}

}  // namespace

ScoConnectionManager::ScoConnectionManager(PeerId peer_id, hci::ConnectionHandle acl_handle,
                                           DeviceAddress peer_address, DeviceAddress local_address,
                                           fxl::WeakPtr<hci::Transport> transport)
    : next_req_id_(0u),
      peer_id_(peer_id),
      local_address_(local_address),
      peer_address_(peer_address),
      acl_handle_(acl_handle),
      transport_(transport),
      weak_ptr_factory_(this) {
  ZX_ASSERT(transport_);

  // register event handlers
  AddEventHandler(hci::kSynchronousConnectionCompleteEventCode,
                  fit::bind_member(this, &ScoConnectionManager::OnSynchronousConnectionComplete));
  AddEventHandler(hci::kConnectionRequestEventCode,
                  fit::bind_member(this, &ScoConnectionManager::OnConnectionRequest));
}

ScoConnectionManager::~ScoConnectionManager() {
  // Remove all event handlers
  for (auto handler_id : event_handler_ids_) {
    transport_->command_channel()->RemoveEventHandler(handler_id);
  }

  // Close all connections
  for (auto [handle, conn] : connections_) {
    conn->Close();
  }

  if (in_progress_request_) {
    bt_log(DEBUG, "gap-sco", "ScoConnectionManager destroyed while request in progress");
    in_progress_request_.reset();
  }
}

ScoConnectionManager::RequestHandle ScoConnectionManager::OpenConnection(
    hci::SynchronousConnectionParameters params, ConnectionCallback callback) {
  return QueueRequest(/*initiator=*/true, params, std::move(callback));
}

ScoConnectionManager::RequestHandle ScoConnectionManager::AcceptConnection(
    hci::SynchronousConnectionParameters params, ConnectionCallback callback) {
  return QueueRequest(/*initiator=*/false, params, std::move(callback));
}

hci::CommandChannel::EventHandlerId ScoConnectionManager::AddEventHandler(
    const hci::EventCode& code, hci::CommandChannel::EventCallback cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto event_id = transport_->command_channel()->AddEventHandler(
      code, [self, callback = std::move(cb)](const auto& event) {
        if (self) {
          return callback(event);
        }
        return hci::CommandChannel::EventCallbackResult::kRemove;
      });
  ZX_ASSERT(event_id);
  event_handler_ids_.push_back(event_id);
  return event_id;
}

hci::CommandChannel::EventCallbackResult ScoConnectionManager::OnSynchronousConnectionComplete(
    const hci::EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci::kSynchronousConnectionCompleteEventCode);

  const auto& params = event.params<hci::SynchronousConnectionCompleteEventParams>();
  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);

  // Ignore events from other peers.
  if (addr != peer_address_) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto status = event.ToStatus();
  if (bt_is_error(status, INFO, "gap-sco", "SCO connection failed to be established (peer: %s)",
                  bt_str(peer_id_))) {
    if (in_progress_request_) {
      CompleteRequest(fit::error(HostError::kFailed));
    }
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  // The controller should only report SCO and eSCO link types (other values are reserved).
  auto link_type = params.link_type;
  if (link_type != hci::LinkType::kSCO && link_type != hci::LinkType::kExtendedSCO) {
    bt_log(ERROR, "gap-sco", "Received SynchronousConnectionComplete event with invalid link type");
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto connection_handle = letoh16(params.connection_handle);
  auto link = hci::Connection::CreateSCO(link_type, connection_handle, local_address_,
                                         peer_address_, transport_);

  if (!in_progress_request_) {
    bt_log(WARN, "gap-sco", "Unexpected SCO connection complete, disconnecting (peer: %s)",
           bt_str(peer_id_));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto conn = ScoConnection::Create(std::move(link), /*deactivated_cb=*/[this, connection_handle] {
    ZX_ASSERT(connections_.erase(connection_handle));
  });

  auto [_, success] = connections_.try_emplace(connection_handle, conn);
  ZX_ASSERT_MSG(success, "SCO connection already exists with handle %#.4x (peer: %s)",
                connection_handle, bt_str(peer_id_));

  CompleteRequest(fit::ok(std::move(conn)));

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult ScoConnectionManager::OnConnectionRequest(
    const hci::EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci::kConnectionRequestEventCode);
  const auto& params = event.params<hci::ConnectionRequestEventParams>();

  // Ignore requests for other link types.
  if (params.link_type != hci::LinkType::kSCO && params.link_type != hci::LinkType::kExtendedSCO) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  // Ignore requests from other peers.
  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);
  if (addr != peer_address_) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  if (!in_progress_request_ || in_progress_request_->initiator) {
    bt_log(INFO, "gap-sco", "reject unexpected SCO connection request (peer: %s)",
           bt_str(peer_id_));

    auto reject =
        hci::CommandPacket::New(hci::kRejectSynchronousConnectionRequest,
                                sizeof(hci::RejectSynchronousConnectionRequestCommandParams));
    auto reject_params =
        reject->mutable_payload<hci::RejectSynchronousConnectionRequestCommandParams>();
    reject_params->bd_addr = params.bd_addr;
    reject_params->reason = hci::StatusCode::kConnectionRejectedBadBdAddr;

    transport_->command_channel()->SendCommand(std::move(reject), nullptr,
                                               hci::kCommandStatusEventCode);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  bt_log(INFO, "gap-bredr", "accepting incoming (e)SCO connection from %s (peer: %s)",
         bt_str(params.bd_addr), bt_str(peer_id_));

  auto accept =
      hci::CommandPacket::New(hci::kEnhancedAcceptSynchronousConnectionRequest,
                              sizeof(hci::EnhancedAcceptSynchronousConnectionRequestCommandParams));
  auto accept_params =
      accept->mutable_payload<hci::EnhancedAcceptSynchronousConnectionRequestCommandParams>();
  accept_params->bd_addr = params.bd_addr;
  accept_params->connection_parameters = ConnectionParametersToLe(in_progress_request_->parameters);
  SendCommandWithStatusCallback(
      std::move(accept), [self = weak_ptr_factory_.GetWeakPtr()](hci::Status status) {
        if (!self || status.is_success()) {
          return;
        }
        bt_is_error(status, DEBUG, "sco", "SCO accept connection command failed");
        self->CompleteRequest(fit::error(HostError::kFailed));
      });

  in_progress_request_->received_request = true;

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

ScoConnectionManager::RequestHandle ScoConnectionManager::QueueRequest(
    bool initiator, hci::SynchronousConnectionParameters params, ConnectionCallback cb) {
  ZX_ASSERT(cb);
  // Cancel the current request.
  queued_request_.reset();

  auto req_id = next_req_id_++;
  queued_request_ = {
      .id = req_id, .initiator = initiator, .parameters = params, .callback = std::move(cb)};

  TryCreateNextConnection();

  return RequestHandle([req_id, self = weak_ptr_factory_.GetWeakPtr()]() {
    if (self) {
      self->CancelRequestWithId(req_id);
    }
  });
}

void ScoConnectionManager::TryCreateNextConnection() {
  // Cancel an in-progress responder request that hasn't received a connection request event yet.
  if (in_progress_request_) {
    CancelRequestWithId(in_progress_request_->id);
  }

  if (in_progress_request_ || !queued_request_) {
    return;
  }

  in_progress_request_ = std::move(queued_request_);
  queued_request_.reset();

  if (in_progress_request_->initiator) {
    bt_log(DEBUG, "gap-sco", "Initiating SCO connection (peer: %s)", bt_str(peer_id_));
    hci::EnhancedSetupSynchronousConnectionCommandParams command;
    command.connection_handle = htole16(acl_handle_);
    command.connection_parameters = ConnectionParametersToLe(in_progress_request_->parameters);

    auto packet =
        hci::CommandPacket::New(hci::kEnhancedSetupSynchronousConnection, sizeof(command));
    *packet->mutable_payload<decltype(command)>() = command;

    auto status_cb = [self = weak_ptr_factory_.GetWeakPtr()](hci::Status status) {
      if (!self || status.is_success()) {
        return;
      }
      bt_is_error(status, DEBUG, "sco", "SCO setup connection command failed");
      self->CompleteRequest(fit::error(HostError::kFailed));
    };

    SendCommandWithStatusCallback(std::move(packet), std::move(status_cb));
  }
}

void ScoConnectionManager::CompleteRequest(ConnectionResult result) {
  ZX_ASSERT(in_progress_request_);
  bt_log(INFO, "gap-sco",
         "Completing SCO connection request (initiator: %d, success: %d, peer: %s)",
         in_progress_request_->initiator, result.is_ok(), bt_str(peer_id_));
  in_progress_request_->callback(std::move(result));
  in_progress_request_.reset();
  TryCreateNextConnection();
}

void ScoConnectionManager::SendCommandWithStatusCallback(
    std::unique_ptr<hci::CommandPacket> command_packet, hci::StatusCallback cb) {
  hci::CommandChannel::CommandCallback command_cb;
  if (cb) {
    command_cb = [cb = std::move(cb)](auto, const hci::EventPacket& event) {
      cb(event.ToStatus());
    };
  }
  transport_->command_channel()->SendCommand(std::move(command_packet), std::move(command_cb));
}

void ScoConnectionManager::CancelRequestWithId(ScoRequestId id) {
  // Cancel queued request if id matches.
  if (queued_request_ && queued_request_->id == id) {
    bt_log(TRACE, "gap-sco", "Cancelling queued request (id: %zu)", id);
    queued_request_.reset();
    return;
  }

  // Cancel in progress request if it is a responder request that hasn't received a connection
  // request yet.
  if (in_progress_request_ && in_progress_request_->id == id && !in_progress_request_->initiator &&
      !in_progress_request_->received_request) {
    bt_log(TRACE, "gap-sco", "Cancelling in progress request (id: %zu)", id);
    CompleteRequest(fit::error(HostError::kCanceled));
  }
}

}  // namespace bt::sco
