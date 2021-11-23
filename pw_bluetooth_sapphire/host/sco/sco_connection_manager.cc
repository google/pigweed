// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection_manager.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"

namespace bt::sco {
namespace {

hci_spec::SynchronousConnectionParameters ConnectionParametersToLe(
    hci_spec::SynchronousConnectionParameters params) {
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

constexpr uint16_t kScoTransportPacketTypes =
    static_cast<uint16_t>(hci_spec::ScoPacketTypeBits::kHv1) |
    static_cast<uint16_t>(hci_spec::ScoPacketTypeBits::kHv2) |
    static_cast<uint16_t>(hci_spec::ScoPacketTypeBits::kHv3);
constexpr uint16_t kEscoTransportPacketTypes =
    static_cast<uint16_t>(hci_spec::ScoPacketTypeBits::kEv3) |
    static_cast<uint16_t>(hci_spec::ScoPacketTypeBits::kEv4) |
    static_cast<uint16_t>(hci_spec::ScoPacketTypeBits::kEv5);

bool ConnectionParametersSupportScoTransport(
    const hci_spec::SynchronousConnectionParameters& params) {
  return params.packet_types & kScoTransportPacketTypes;
}

bool ConnectionParametersSupportEscoTransport(
    const hci_spec::SynchronousConnectionParameters& params) {
  return params.packet_types & kEscoTransportPacketTypes;
}

}  // namespace

ScoConnectionManager::ScoConnectionManager(PeerId peer_id, hci_spec::ConnectionHandle acl_handle,
                                           DeviceAddress peer_address, DeviceAddress local_address,
                                           fxl::WeakPtr<hci::Transport> transport)
    : next_req_id_(0u),
      peer_id_(peer_id),
      local_address_(local_address),
      peer_address_(peer_address),
      acl_handle_(acl_handle),
      transport_(std::move(transport)),
      weak_ptr_factory_(this) {
  ZX_ASSERT(transport_);

  AddEventHandler(hci_spec::kSynchronousConnectionCompleteEventCode,
                  fit::bind_member(this, &ScoConnectionManager::OnSynchronousConnectionComplete));
  AddEventHandler(hci_spec::kConnectionRequestEventCode,
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

  if (queued_request_) {
    CancelRequestWithId(queued_request_->id);
  }

  if (in_progress_request_) {
    bt_log(DEBUG, "gap-sco", "ScoConnectionManager destroyed while request in progress");
    // Clear in_progress_request_ before calling callback to prevent calls to
    // CompleteRequest() during execution of the callback (e.g. due to destroying the
    // RequestHandle).
    ConnectionRequest request = std::move(in_progress_request_.value());
    in_progress_request_.reset();
    request.callback(fpromise::error(HostError::kCanceled));
  }
}

ScoConnectionManager::RequestHandle ScoConnectionManager::OpenConnection(
    hci_spec::SynchronousConnectionParameters parameters, OpenConnectionCallback callback) {
  return QueueRequest(/*initiator=*/true, {parameters},
                      [cb = std::move(callback)](ConnectionResult result) mutable {
                        // Convert result type.
                        if (result.is_error()) {
                          cb(fpromise::error(result.take_error()));
                          return;
                        }
                        cb(fpromise::ok(std::move(result.value().first)));
                      });
}

ScoConnectionManager::RequestHandle ScoConnectionManager::AcceptConnection(
    std::vector<hci_spec::SynchronousConnectionParameters> parameters,
    AcceptConnectionCallback callback) {
  return QueueRequest(/*initiator=*/false, std::move(parameters), std::move(callback));
}

hci::CommandChannel::EventHandlerId ScoConnectionManager::AddEventHandler(
    const hci_spec::EventCode& code, hci::CommandChannel::EventCallback cb) {
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
  ZX_ASSERT(event.event_code() == hci_spec::kSynchronousConnectionCompleteEventCode);

  const auto& params = event.params<hci_spec::SynchronousConnectionCompleteEventParams>();
  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);

  // Ignore events from other peers.
  if (addr != peer_address_) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto status = event.ToStatus();
  if (bt_is_error(
          status, INFO, "gap-sco",
          "SCO connection failed to be established; trying next parameters if available (peer: %s)",
          bt_str(peer_id_))) {
    // A request must be in progress for this event to be generated.
    CompleteRequestOrTryNextParameters(fpromise::error(HostError::kFailed));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  // The controller should only report SCO and eSCO link types (other values are reserved).
  auto link_type = params.link_type;
  if (link_type != hci_spec::LinkType::kSCO && link_type != hci_spec::LinkType::kExtendedSCO) {
    bt_log(ERROR, "gap-sco", "Received SynchronousConnectionComplete event with invalid link type");
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto connection_handle = letoh16(params.connection_handle);
  auto link = hci::Connection::CreateSCO(link_type, connection_handle, local_address_,
                                         peer_address_, transport_);

  if (!in_progress_request_) {
    bt_log(ERROR, "gap-sco", "Unexpected SCO connection complete, disconnecting (peer: %s)",
           bt_str(peer_id_));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto conn = ScoConnection::Create(std::move(link), /*deactivated_cb=*/[this, connection_handle] {
    ZX_ASSERT(connections_.erase(connection_handle));
  });

  auto [_, success] = connections_.try_emplace(connection_handle, conn);
  ZX_ASSERT_MSG(success, "SCO connection already exists with handle %#.4x (peer: %s)",
                connection_handle, bt_str(peer_id_));

  CompleteRequest(
      fpromise::ok(std::make_pair(std::move(conn), in_progress_request_->current_param_index)));

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult ScoConnectionManager::OnConnectionRequest(
    const hci::EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci_spec::kConnectionRequestEventCode);
  const auto& params = event.params<hci_spec::ConnectionRequestEventParams>();

  // Ignore requests for other link types.
  if (params.link_type != hci_spec::LinkType::kSCO &&
      params.link_type != hci_spec::LinkType::kExtendedSCO) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  // Ignore requests from other peers.
  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);
  if (addr != peer_address_) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  if (!in_progress_request_ || in_progress_request_->initiator) {
    bt_log(INFO, "sco", "reject unexpected %s connection request (peer: %s)",
           hci_spec::LinkTypeToString(params.link_type).c_str(), bt_str(peer_id_));
    SendRejectConnectionCommand(params.bd_addr, hci_spec::StatusCode::kConnectionRejectedBadBdAddr);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  // Skip to the next parameters that support the requested link type. The controller rejects
  // parameters that don't include packet types for the requested link type.
  if ((params.link_type == hci_spec::LinkType::kSCO && !FindNextParametersThatSupportSco()) ||
      (params.link_type == hci_spec::LinkType::kExtendedSCO &&
       !FindNextParametersThatSupportEsco())) {
    bt_log(DEBUG, "sco",
           "in progress request parameters don't support the requested transport (%s); rejecting",
           hci_spec::LinkTypeToString(params.link_type).c_str());
    // The controller will send an HCI Synchronous Connection Complete event, so the request will be
    // completed then.
    SendRejectConnectionCommand(params.bd_addr,
                                hci_spec::StatusCode::kConnectionRejectedLimitedResources);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  bt_log(INFO, "sco", "accepting incoming %s connection from %s (peer: %s)",
         hci_spec::LinkTypeToString(params.link_type).c_str(), bt_str(params.bd_addr),
         bt_str(peer_id_));

  auto accept = hci::CommandPacket::New(
      hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
      sizeof(hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandParams));
  auto accept_params =
      accept->mutable_payload<hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandParams>();
  accept_params->bd_addr = params.bd_addr;
  accept_params->connection_parameters = ConnectionParametersToLe(
      in_progress_request_->parameters[in_progress_request_->current_param_index]);
  SendCommandWithStatusCallback(std::move(accept), [self = weak_ptr_factory_.GetWeakPtr(),
                                                    peer_id = peer_id_](hci::Status status) {
    if (!self || status.is_success()) {
      return;
    }
    bt_is_error(
        status, WARN, "sco",
        "enhanced accept SCO connection command failed, waiting for connection complete (peer: %s",
        bt_str(peer_id));
    // Do not complete the request here. Wait for HCI_Synchronous_Connection_Complete event,
    // which should be received after Connection_Accept_Timeout with status
    // kConnectionAcceptTimeoutExceeded.
  });

  in_progress_request_->received_request = true;

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

bool ScoConnectionManager::FindNextParametersThatSupportSco() {
  ZX_ASSERT(in_progress_request_);
  while (in_progress_request_->current_param_index < in_progress_request_->parameters.size()) {
    hci_spec::SynchronousConnectionParameters& params =
        in_progress_request_->parameters[in_progress_request_->current_param_index];
    if (ConnectionParametersSupportScoTransport(params)) {
      return true;
    }
    in_progress_request_->current_param_index++;
  }
  return false;
}

bool ScoConnectionManager::FindNextParametersThatSupportEsco() {
  ZX_ASSERT(in_progress_request_);
  while (in_progress_request_->current_param_index < in_progress_request_->parameters.size()) {
    hci_spec::SynchronousConnectionParameters& params =
        in_progress_request_->parameters[in_progress_request_->current_param_index];
    if (ConnectionParametersSupportEscoTransport(params)) {
      return true;
    }
    in_progress_request_->current_param_index++;
  }
  return false;
}

ScoConnectionManager::RequestHandle ScoConnectionManager::QueueRequest(
    bool initiator, std::vector<hci_spec::SynchronousConnectionParameters> params,
    ConnectionCallback cb) {
  ZX_ASSERT(cb);

  if (params.empty()) {
    cb(fpromise::error(HostError::kInvalidParameters));
    return RequestHandle([]() {});
  }

  if (queued_request_) {
    CancelRequestWithId(queued_request_->id);
  }

  auto req_id = next_req_id_++;
  queued_request_ = {.id = req_id,
                     .initiator = initiator,
                     .received_request = false,
                     .parameters = std::move(params),
                     .callback = std::move(cb)};

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
    hci_spec::EnhancedSetupSynchronousConnectionCommandParams command;
    command.connection_handle = htole16(acl_handle_);
    command.connection_parameters = ConnectionParametersToLe(
        in_progress_request_->parameters[in_progress_request_->current_param_index]);

    auto packet =
        hci::CommandPacket::New(hci_spec::kEnhancedSetupSynchronousConnection, sizeof(command));
    *packet->mutable_payload<decltype(command)>() = command;

    auto status_cb = [self = weak_ptr_factory_.GetWeakPtr()](hci::Status status) {
      if (!self || status.is_success()) {
        return;
      }
      bt_is_error(status, WARN, "sco", "SCO setup connection command failed");
      self->CompleteRequest(fpromise::error(HostError::kFailed));
    };

    SendCommandWithStatusCallback(std::move(packet), std::move(status_cb));
  }
}

void ScoConnectionManager::CompleteRequestOrTryNextParameters(ConnectionResult result) {
  ZX_ASSERT(in_progress_request_);

  // Multiple parameter attempts are not supported for initiator requests.
  if (result.is_ok() || in_progress_request_->initiator) {
    CompleteRequest(std::move(result));
    return;
  }

  // Check if all accept request parameters have been exhausted.
  if (in_progress_request_->current_param_index + 1 >= in_progress_request_->parameters.size()) {
    bt_log(DEBUG, "sco", "all accept SCO parameters exhausted");
    CompleteRequest(fpromise::error(HostError::kParametersRejected));
    return;
  }

  // If a request was queued after the connection request event (blocking cancelation at that time),
  // cancel the current request.
  if (queued_request_) {
    CompleteRequest(fpromise::error(HostError::kCanceled));
    return;
  }

  // Wait for the next inbound connection request and accept it with the next parameters.
  in_progress_request_->received_request = false;
  in_progress_request_->current_param_index++;
}

void ScoConnectionManager::CompleteRequest(ConnectionResult result) {
  ZX_ASSERT(in_progress_request_);
  bt_log(INFO, "gap-sco",
         "Completing SCO connection request (initiator: %d, success: %d, peer: %s)",
         in_progress_request_->initiator, result.is_ok(), bt_str(peer_id_));
  // Clear in_progress_request_ before calling callback to prevent additional calls to
  // CompleteRequest() during execution of the callback (e.g. due to destroying the RequestHandle).
  ConnectionRequest request = std::move(in_progress_request_.value());
  in_progress_request_.reset();
  request.callback(std::move(result));
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

void ScoConnectionManager::SendRejectConnectionCommand(DeviceAddressBytes addr,
                                                       hci_spec::StatusCode reason) {
  // The reject command has a small range of allowed reasons (the controller sends "Invalid HCI
  // Command Parameters" for other reasons).
  ZX_ASSERT_MSG(reason == hci_spec::StatusCode::kConnectionRejectedLimitedResources ||
                    reason == hci_spec::StatusCode::kConnectionRejectedSecurity ||
                    reason == hci_spec::StatusCode::kConnectionRejectedBadBdAddr,
                "Tried to send invalid reject reason: %s",
                hci_spec::StatusCodeToString(reason).c_str());

  auto reject =
      hci::CommandPacket::New(hci_spec::kRejectSynchronousConnectionRequest,
                              sizeof(hci_spec::RejectSynchronousConnectionRequestCommandParams));
  auto reject_params =
      reject->mutable_payload<hci_spec::RejectSynchronousConnectionRequestCommandParams>();
  reject_params->bd_addr = addr;
  reject_params->reason = reason;

  transport_->command_channel()->SendCommand(std::move(reject), nullptr,
                                             hci_spec::kCommandStatusEventCode);
}

void ScoConnectionManager::CancelRequestWithId(ScoRequestId id) {
  // Cancel queued request if id matches.
  if (queued_request_ && queued_request_->id == id) {
    bt_log(INFO, "gap-sco", "Cancelling queued SCO request (id: %zu)", id);
    // Clear queued_request_ before calling callback to prevent calls to
    // CancelRequestWithId() during execution of the callback (e.g. due to destroying the
    // RequestHandle).
    ConnectionRequest request = std::move(queued_request_.value());
    queued_request_.reset();
    request.callback(fpromise::error(HostError::kCanceled));
    return;
  }

  // Cancel in progress request if it is a responder request that hasn't received a connection
  // request yet.
  if (in_progress_request_ && in_progress_request_->id == id && !in_progress_request_->initiator &&
      !in_progress_request_->received_request) {
    bt_log(INFO, "gap-sco", "Cancelling in progress SCO request (id: %zu)", id);
    CompleteRequest(fpromise::error(HostError::kCanceled));
  }
}

}  // namespace bt::sco
