// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"

#include <mutex>
#include <optional>

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"
#include "pw_multibuf/allocator.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

L2capSignalingChannel::L2capSignalingChannel(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t fixed_cid)
    : BasicL2capChannel(l2cap_channel_manager,
                        /*rx_multibuf_allocator=*/nullptr,
                        /*connection_handle=*/connection_handle,
                        /*transport*/ transport,
                        /*local_cid=*/fixed_cid,
                        /*remote_cid=*/fixed_cid,
                        /*payload_from_controller_fn=*/nullptr,
                        /*payload_from_host_fn=*/nullptr,
                        /*event_fn=*/nullptr),
      l2cap_channel_manager_(l2cap_channel_manager) {}

L2capSignalingChannel& L2capSignalingChannel::operator=(
    L2capSignalingChannel&& other) {
  std::lock_guard lock(mutex_);
  std::lock_guard other_lock(other.mutex_);
  pending_connections_ = std::move(other.pending_connections_);

  BasicL2capChannel::operator=(std::move(other));
  return *this;
}

bool L2capSignalingChannel::DoHandlePduFromController(
    pw::span<uint8_t> cframe) {
  Result<emboss::CFrameView> cframe_view =
      MakeEmbossView<emboss::CFrameView>(cframe);
  if (!cframe_view.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for C-frame. So will forward to host without "
        "processing.");
    return false;
  }

  // TODO: https://pwbug.dev/360929142 - "If a device receives a C-frame that
  // exceeds its L2CAP_SIG_MTU_SIZE then it shall send an
  // L2CAP_COMMAND_REJECT_RSP packet containing the supported
  // L2CAP_SIG_MTU_SIZE." We should consider taking the signaling MTU in the
  // ProxyHost constructor.
  return OnCFramePayload(
      Direction::kFromController,
      pw::span(cframe_view->payload().BackingStorage().data(),
               cframe_view->payload().BackingStorage().SizeInBytes()));
}

bool L2capSignalingChannel::HandlePduFromHost(pw::span<uint8_t> cframe) {
  Result<emboss::CFrameView> cframe_view =
      MakeEmbossView<emboss::CFrameView>(cframe);
  if (!cframe_view.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for C-frame. So will forward to controller "
        "without processing.");
    return false;
  }

  return OnCFramePayload(
      Direction::kFromHost,
      pw::span(cframe_view->payload().BackingStorage().data(),
               cframe_view->payload().BackingStorage().SizeInBytes()));
}

bool L2capSignalingChannel::HandleL2capSignalingCommand(
    Direction direction, emboss::L2capSignalingCommandView cmd) {
  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (cmd.command_header().code().Read()) {
    case emboss::L2capSignalingPacketCode::CONNECTION_REQ: {
      Result<emboss::L2capConnectionReqView> connection_req_cmd =
          MakeEmbossView<emboss::L2capConnectionReqView>(
              cmd.BackingStorage().data(), cmd.SizeInBytes());
      if (!connection_req_cmd.ok()) {
        return false;
      }
      HandleConnectionReq(direction, *connection_req_cmd);
      return false;
    }
    case emboss::L2capSignalingPacketCode::CONNECTION_RSP: {
      Result<emboss::L2capConnectionRspView> connection_rsp_cmd =
          MakeEmbossView<emboss::L2capConnectionRspView>(
              cmd.BackingStorage().data(), cmd.SizeInBytes());
      if (!connection_rsp_cmd.ok()) {
        return false;
      }
      HandleConnectionRsp(direction, *connection_rsp_cmd);
      return false;
    }
    case emboss::L2capSignalingPacketCode::DISCONNECTION_REQ: {
      Result<emboss::L2capDisconnectionReqView> disconnection_req_cmd =
          MakeEmbossView<emboss::L2capDisconnectionReqView>(
              cmd.BackingStorage().data(), cmd.SizeInBytes());
      if (!disconnection_req_cmd.ok()) {
        return false;
      }
      HandleDisconnectionReq(direction, *disconnection_req_cmd);
      return false;
    }
    case emboss::L2capSignalingPacketCode::DISCONNECTION_RSP: {
      Result<emboss::L2capDisconnectionRspView> disconnection_rsp_cmd =
          MakeEmbossView<emboss::L2capDisconnectionRspView>(
              cmd.BackingStorage().data(), cmd.SizeInBytes());
      if (!disconnection_rsp_cmd.ok()) {
        return false;
      }
      HandleDisconnectionRsp(direction, *disconnection_rsp_cmd);
      return false;
    }
    case emboss::L2capSignalingPacketCode::FLOW_CONTROL_CREDIT_IND: {
      return HandleFlowControlCreditInd(
          emboss::MakeL2capFlowControlCreditIndView(cmd.BackingStorage().data(),
                                                    cmd.SizeInBytes()));
    }
    default: {
      return false;
    }
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

void L2capSignalingChannel::HandleConnectionReq(
    Direction direction, emboss::L2capConnectionReqView cmd) {
  std::lock_guard lock(mutex_);
  if (pending_connections_.full()) {
    PW_LOG_ERROR("Reached max number of tracked pending l2cap connections.");
    return;
  }
  pending_connections_.push_back({.direction = direction,
                                  .source_cid = cmd.source_cid().Read(),
                                  .psm = cmd.psm().Read()});
}

void L2capSignalingChannel::HandleConnectionRsp(
    Direction direction, emboss::L2capConnectionRspView cmd) {
  std::lock_guard lock(mutex_);
  // Match this response to an existing pending connection request. Flip the
  // response's direction since we will be looking for a match with the
  // request's direction
  Direction request_direction = direction == Direction::kFromHost
                                    ? Direction::kFromController
                                    : Direction::kFromHost;

  struct {
    Direction direction;
    uint16_t source_cid;
  } match_info = {.direction = request_direction,
                  .source_cid = cmd.source_cid().Read()};

  auto match = [&match_info](const PendingConnection& pending) {
    // TODO: https://pwbug.dev/379558046 - Consider using identifier instead to
    // match request to response
    return pending.source_cid == match_info.source_cid &&
           pending.direction == match_info.direction;
  };
  auto pending_it = std::find_if(
      pending_connections_.begin(), pending_connections_.end(), match);
  if (pending_it == pending_connections_.end()) {
    PW_LOG_WARN("No match found for l2cap connection");
    return;
  }

  switch (cmd.result().Read()) {
    case emboss::L2capConnectionRspResultCode::SUCCESSFUL: {
      // We now have complete connection info
      l2cap_channel_manager_.HandleConnectionComplete(
          L2capChannelConnectionInfo{
              .direction = request_direction,
              .psm = pending_it->psm,
              .connection_handle = connection_handle(),
              .remote_cid = cmd.source_cid().Read(),
              .local_cid = cmd.destination_cid().Read(),
          });
      pending_connections_.erase(pending_it);

      break;
    }
    case emboss::L2capConnectionRspResultCode::PENDING: {
      // Leave pending if result is pending.
      break;
    }
    case emboss::L2capConnectionRspResultCode::PSM_NOT_SUPPORTED:
    case emboss::L2capConnectionRspResultCode::SECURITY_BLOCK:
    case emboss::L2capConnectionRspResultCode::NO_RESOURCES_AVAILABLE:
    case emboss::L2capConnectionRspResultCode::INVALID_SOURCE_CID:
    case emboss::L2capConnectionRspResultCode::SOURCE_CID_ALREADY_ALLOCATED:
    default: {
      // All other codes mean the connection has failed.
      pending_connections_.erase(pending_it);
      break;
    }
  }
}

void L2capSignalingChannel::HandleDisconnectionReq(
    Direction, emboss::L2capDisconnectionReqView) {
  // TODO: https://pwbug.dev/379558046 - On reception of this event we should
  // stop queueing data on this connection.
}

void L2capSignalingChannel::HandleDisconnectionRsp(
    Direction, emboss::L2capDisconnectionRspView cmd) {
  l2cap_channel_manager_.HandleDisconnectionComplete(
      L2capStatusTracker::DisconnectParams{
          .connection_handle = connection_handle(),
          .remote_cid = cmd.source_cid().Read(),
          .local_cid = cmd.destination_cid().Read()});
}

bool L2capSignalingChannel::HandleFlowControlCreditInd(
    emboss::L2capFlowControlCreditIndView cmd) {
  if (!cmd.IsComplete()) {
    PW_LOG_ERROR(
        "Buffer is too small for L2CAP_FLOW_CONTROL_CREDIT_IND. So will "
        "forward to host without processing.");
    return false;
  }

  L2capChannel* found_channel = l2cap_channel_manager_.FindChannelByRemoteCid(
      connection_handle(), cmd.cid().Read());
  if (found_channel) {
    // If this L2CAP_FLOW_CONTROL_CREDIT_IND is addressed to a channel managed
    // by the proxy, it must be an L2CAP connection-oriented channel.
    // TODO: https://pwbug.dev/360929142 - Validate type in case remote peer
    // sends indication addressed to wrong CID.
    L2capCocInternal* coc_ptr = static_cast<L2capCocInternal*>(found_channel);
    coc_ptr->AddTxCredits(cmd.credits().Read());
    return true;
  }
  return false;
}

namespace {

// TODO: https://pwbug.dev/389724307 - Move to pw utility function once created.
pw::span<uint8_t> AsUint8Span(ByteSpan s) {
  return {reinterpret_cast<uint8_t*>(s.data()), s.size_bytes()};
}

}  // namespace

Status L2capSignalingChannel::SendFlowControlCreditInd(
    uint16_t cid,
    uint16_t credits,
    multibuf::MultiBufAllocator& multibuf_allocator) {
  if (cid == 0) {
    PW_LOG_ERROR("Tried to send signaling packet on invalid CID 0x0.");
    return Status::InvalidArgument();
  }

  std::optional<pw::multibuf::MultiBuf> command =
      multibuf_allocator.AllocateContiguous(
          emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  if (!command.has_value()) {
    PW_LOG_ERROR(
        "btproxy: SendFlowControlCreditInd unable to allocate command buffer "
        "from provided multibuf_allocator. cid: %#x, credits: %#x",
        cid,
        credits);
    return Status::Unavailable();
  }
  std::optional<ByteSpan> command_span = command->ContiguousSpan();

  Result<emboss::L2capFlowControlCreditIndWriter> command_view =
      MakeEmbossWriter<emboss::L2capFlowControlCreditIndWriter>(
          AsUint8Span(command_span.value()));
  PW_CHECK(command_view->IsComplete());

  command_view->command_header().code().Write(
      emboss::L2capSignalingPacketCode::FLOW_CONTROL_CREDIT_IND);
  command_view->command_header().identifier().Write(
      GetNextIdentifierAndIncrement());
  command_view->command_header().data_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes() -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  command_view->cid().Write(cid);
  command_view->credits().Write(credits);
  PW_CHECK(command_view->Ok());

  StatusWithMultiBuf s = Write(*std::move(command));

  return s.status;
}

uint8_t L2capSignalingChannel::GetNextIdentifierAndIncrement() {
  if (next_identifier_ == UINT8_MAX) {
    next_identifier_ = 1;
    return UINT8_MAX;
  }
  return next_identifier_++;
}

}  // namespace pw::bluetooth::proxy
