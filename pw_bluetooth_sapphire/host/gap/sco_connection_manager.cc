// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection_manager.h"

#include "lib/async/default.h"

namespace bt::gap {
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
  return params;
}

}  // namespace

ScoConnectionManager::ScoConnectionManager(PeerId peer_id, hci::ConnectionHandle acl_handle,
                                           DeviceAddress peer_address, DeviceAddress local_address,
                                           fxl::WeakPtr<hci::Transport> transport)
    : peer_id_(peer_id),
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

  // Cancel in progress request.
  if (in_progress_request_) {
    bt_log(DEBUG, "gap-sco", "ScoConnectionManager destroyed while request in progress");
    in_progress_request_->callback(nullptr);
    in_progress_request_.reset();
  }

  // Cancel queued requests.
  while (!connection_requests_.empty()) {
    connection_requests_.front().callback(nullptr);
    connection_requests_.pop();
  }
}

void ScoConnectionManager::OpenConnection(hci::SynchronousConnectionParameters params,
                                          ConnectionCallback callback) {
  ZX_ASSERT(callback);
  connection_requests_.push(
      {.initiator = true, .parameters = params, .callback = std::move(callback)});
  TryCreateNextConnection();
}

void ScoConnectionManager::AcceptConnection(hci::SynchronousConnectionParameters params,
                                            ConnectionCallback callback) {
  ZX_ASSERT(callback);
  connection_requests_.push(
      {.initiator = false, .parameters = params, .callback = std::move(callback)});
  TryCreateNextConnection();
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
      CompleteRequest(nullptr);
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

  CompleteRequest(std::move(conn));

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

  auto accept =
      hci::CommandPacket::New(hci::kEnhancedAcceptSynchronousConnectionRequest,
                              sizeof(hci::EnhancedAcceptSynchronousConnectionRequestCommandParams));
  auto accept_params =
      accept->mutable_payload<hci::EnhancedAcceptSynchronousConnectionRequestCommandParams>();
  accept_params->bd_addr = params.bd_addr;
  accept_params->connection_parameters = ConnectionParametersToLe(in_progress_request_->parameters);
  SendCommandWithStatusCallback(std::move(accept),
                                [self = weak_ptr_factory_.GetWeakPtr()](hci::Status status) {
                                  if (!self || status.is_success()) {
                                    return;
                                  }
                                  self->CompleteRequest(nullptr);
                                });

  in_progress_request_->received_request = true;

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

void ScoConnectionManager::TryCreateNextConnection() {
  // Cancel an in-progress responder request that hasn't received a connection request event yet.
  if (in_progress_request_ && !in_progress_request_->initiator &&
      !in_progress_request_->received_request) {
    bt_log(DEBUG, "gap-sco", "Cancelling in progress responder SCO connection due to new request");
    in_progress_request_->callback(nullptr);
    in_progress_request_.reset();
  }

  if (in_progress_request_ || connection_requests_.empty()) {
    return;
  }

  in_progress_request_ = std::move(connection_requests_.front());
  connection_requests_.pop();

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
      self->CompleteRequest(nullptr);
    };

    SendCommandWithStatusCallback(std::move(packet), std::move(status_cb));
  }
}

void ScoConnectionManager::CompleteRequest(fbl::RefPtr<ScoConnection> connection) {
  ZX_ASSERT(in_progress_request_);
  bt_log(INFO, "gap-sco",
         "Completing SCO connection request (initiator: %d, success: %d, peer: %s)",
         in_progress_request_->initiator, static_cast<bool>(connection), bt_str(peer_id_));
  in_progress_request_->callback(std::move(connection));
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

}  // namespace bt::gap
