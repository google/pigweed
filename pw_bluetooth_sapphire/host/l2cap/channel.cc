// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel.h"

#include <lib/async/default.h>

#include <functional>
#include <memory>
#include <utility>

#include "lib/fit/result.h"
#include "logical_link.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/trace.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_rx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_tx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_engines.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::l2cap {

namespace hci_android = bt::hci_spec::vendor::android;
using pw::bluetooth::AclPriority;

Channel::Channel(ChannelId id, ChannelId remote_id, bt::LinkType link_type,
                 hci_spec::ConnectionHandle link_handle, ChannelInfo info)
    : id_(id),
      remote_id_(remote_id),
      link_type_(link_type),
      link_handle_(link_handle),
      info_(info),
      requested_acl_priority_(AclPriority::kNormal),
      a2dp_offload_status_(A2dpOffloadStatus::kStopped) {
  BT_DEBUG_ASSERT(id_);
  BT_DEBUG_ASSERT(link_type_ == bt::LinkType::kLE || link_type_ == bt::LinkType::kACL);
}

namespace internal {

namespace {

constexpr const char* kInspectLocalIdPropertyName = "local_id";
constexpr const char* kInspectRemoteIdPropertyName = "remote_id";
constexpr const char* kInspectPsmPropertyName = "psm";

}  // namespace

std::unique_ptr<ChannelImpl> ChannelImpl::CreateFixedChannel(
    ChannelId id, fxl::WeakPtr<internal::LogicalLink> link,
    fxl::WeakPtr<hci::CommandChannel> cmd_channel) {
  // A fixed channel's endpoints have the same local and remote identifiers.
  // Setting the ChannelInfo MTU to kMaxMTU effectively cancels any L2CAP-level MTU enforcement for
  // services which operate over fixed channels. Such services often define minimum MTU values in
  // their specification, so they are required to respect these MTUs internally by:
  //   1.) never sending packets larger than their spec-defined MTU.
  //   2.) handling inbound PDUs which are larger than their spec-defined MTU appropriately.
  return std::unique_ptr<ChannelImpl>(new ChannelImpl(
      id, id, link, ChannelInfo::MakeBasicMode(kMaxMTU, kMaxMTU), std::move(cmd_channel)));
}

std::unique_ptr<ChannelImpl> ChannelImpl::CreateDynamicChannel(
    ChannelId id, ChannelId peer_id, fxl::WeakPtr<internal::LogicalLink> link, ChannelInfo info,
    fxl::WeakPtr<hci::CommandChannel> cmd_channel) {
  return std::unique_ptr<ChannelImpl>(
      new ChannelImpl(id, peer_id, link, info, std::move(cmd_channel)));
}

ChannelImpl::ChannelImpl(ChannelId id, ChannelId remote_id,
                         fxl::WeakPtr<internal::LogicalLink> link, ChannelInfo info,
                         fxl::WeakPtr<hci::CommandChannel> cmd_channel)
    : Channel(id, remote_id, link->type(), link->handle(), info),
      active_(false),
      link_(link),
      cmd_channel_(std::move(cmd_channel)),
      weak_ptr_factory_(this) {
  BT_ASSERT(link_);
  BT_ASSERT_MSG(
      info_.mode == ChannelMode::kBasic || info_.mode == ChannelMode::kEnhancedRetransmission,
      "Channel constructed with unsupported mode: %hhu\n", info.mode);

  if (info_.mode == ChannelMode::kBasic) {
    rx_engine_ = std::make_unique<BasicModeRxEngine>();
    tx_engine_ = std::make_unique<BasicModeTxEngine>(
        id, max_tx_sdu_size(), fit::bind_member<&ChannelImpl::SendFrame>(this));
  } else {
    // Must capture |link| and not |link_| to avoid having to take |mutex_|.
    auto connection_failure_cb = [link] {
      if (link) {
        // |link| is expected to ignore this call if it has been closed.
        link->SignalError();
      }
    };
    std::tie(rx_engine_, tx_engine_) = MakeLinkedEnhancedRetransmissionModeEngines(
        id, max_tx_sdu_size(), info_.max_transmissions, info_.n_frames_in_tx_window,
        fit::bind_member<&ChannelImpl::SendFrame>(this), std::move(connection_failure_cb));
  }
}

const sm::SecurityProperties ChannelImpl::security() {
  if (link_) {
    return link_->security();
  }
  return sm::SecurityProperties();
}

bool ChannelImpl::Activate(RxCallback rx_callback, ClosedCallback closed_callback) {
  BT_ASSERT(rx_callback);
  BT_ASSERT(closed_callback);

  // Activating on a closed link has no effect. We also clear this on
  // deactivation to prevent a channel from being activated more than once.
  if (!link_)
    return false;

  BT_ASSERT(!active_);
  active_ = true;
  rx_cb_ = std::move(rx_callback);
  closed_cb_ = std::move(closed_callback);

  // Route the buffered packets.
  if (!pending_rx_sdus_.empty()) {
    TRACE_DURATION("bluetooth", "ChannelImpl::Activate pending drain");
    // Channel may be destroyed in rx_cb_, so we need to check self after calling rx_cb_.
    auto self = weak_ptr_factory_.GetWeakPtr();
    auto pending = std::move(pending_rx_sdus_);
    while (self && !pending.empty()) {
      TRACE_FLOW_END("bluetooth", "ChannelImpl::HandleRxPdu queued", pending.size());
      rx_cb_(std::move(pending.front()));
      pending.pop();
    }
  }

  return true;
}

void ChannelImpl::Deactivate() {
  bt_log(TRACE, "l2cap", "deactivating channel (link: %#.4x, id: %#.4x)", link_handle(), id());

  // De-activating on a closed link has no effect.
  if (!link_ || !active_) {
    link_ = nullptr;
    return;
  }

  auto link = link_;

  CleanUp();

  // |link| is expected to ignore this call if it has been closed.
  link->RemoveChannel(this, /*removed_cb=*/[] {});
}

void ChannelImpl::SignalLinkError() {
  // Cannot signal an error on a closed or deactivated link.
  if (!link_ || !active_)
    return;

  // |link_| is expected to ignore this call if it has been closed.
  link_->SignalError();
}

bool ChannelImpl::Send(ByteBufferPtr sdu) {
  BT_DEBUG_ASSERT(sdu);

  TRACE_DURATION("bluetooth", "l2cap:channel_send", "handle", link_->handle(), "id", id());

  if (!link_) {
    bt_log(ERROR, "l2cap", "cannot send SDU on a closed link");
    return false;
  }

  // Drop the packet if the channel is inactive.
  if (!active_)
    return false;

  return tx_engine_->QueueSdu(std::move(sdu));
}

void ChannelImpl::UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback) {
  BT_ASSERT(callback);

  if (!link_ || !active_) {
    bt_log(DEBUG, "l2cap", "Ignoring security request on inactive channel");
    return;
  }

  link_->UpgradeSecurity(level, std::move(callback));
}

void ChannelImpl::RequestAclPriority(AclPriority priority,
                                     fit::callback<void(fit::result<fit::failed>)> callback) {
  if (!link_ || !active_) {
    bt_log(DEBUG, "l2cap", "Ignoring ACL priority request on inactive channel");
    callback(fit::failed());
    return;
  }

  link_->RequestAclPriority(this, priority,
                            [self = weak_ptr_factory_.GetWeakPtr(), priority,
                             cb = std::move(callback)](auto result) mutable {
                              if (self && result.is_ok()) {
                                self->requested_acl_priority_ = priority;
                              }
                              cb(result);
                            });
}

void ChannelImpl::SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                                hci::ResultCallback<> callback) {
  BT_ASSERT(link_type_ == bt::LinkType::kACL);

  // Channel may be inactive if this method is called before activation.
  if (!link_) {
    bt_log(DEBUG, "l2cap", "Ignoring %s on closed channel", __FUNCTION__);
    callback(ToResult(hci_spec::StatusCode::COMMAND_DISALLOWED));
    return;
  }

  auto cb_wrapper = [self = weak_ptr_factory_.GetWeakPtr(), cb = std::move(callback),
                     flush_timeout](auto result) mutable {
    if (!self) {
      cb(ToResult(hci_spec::StatusCode::UNSPECIFIED_ERROR));
      return;
    }

    if (result.is_ok()) {
      self->info_.flush_timeout = flush_timeout;
    }

    cb(result);
  };

  link_->SetBrEdrAutomaticFlushTimeout(flush_timeout, std::move(cb_wrapper));
}

void ChannelImpl::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_.node = parent.CreateChild(name);
  if (info_.psm) {
    inspect_.psm =
        inspect_.node.CreateString(kInspectPsmPropertyName, PsmToString(info_.psm.value()));
  }
  inspect_.local_id = inspect_.node.CreateString(kInspectLocalIdPropertyName,
                                                 bt_lib_cpp_string::StringPrintf("%#.4x", id()));
  inspect_.remote_id = inspect_.node.CreateString(
      kInspectRemoteIdPropertyName, bt_lib_cpp_string::StringPrintf("%#.4x", remote_id()));
}

void ChannelImpl::StartA2dpOffload(const A2dpOffloadConfiguration* config,
                                   hci::ResultCallback<> callback) {
  if (a2dp_offload_status_ == A2dpOffloadStatus::kStarted ||
      a2dp_offload_status_ == A2dpOffloadStatus::kPending) {
    bt_log(WARN, "hci", "A2DP offload already started (status: %hhu)", a2dp_offload_status_);
    callback(ToResult(HostError::kInProgress));
    return;
  }

  std::unique_ptr<hci::CommandPacket> packet = hci::CommandPacket::New(
      hci_android::kA2dpOffloadCommand, sizeof(hci_android::StartA2dpOffloadCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_android::StartA2dpOffloadCommandParams>();
  payload->opcode = hci_android::kStartA2dpOffloadCommandSubopcode;
  payload->codec_type = static_cast<hci_android::A2dpCodecType>(htole32(config->codec));
  payload->max_latency = htole16(config->max_latency);
  payload->scms_t_enable = config->scms_t_enable;
  payload->sampling_frequency =
      static_cast<hci_android::A2dpSamplingFrequency>(htole32(config->sampling_frequency));
  payload->bits_per_sample = config->bits_per_sample;
  payload->channel_mode = config->channel_mode;
  payload->encoded_audio_bitrate = htole32(config->encoded_audio_bit_rate);
  payload->connection_handle = htole16(link_handle());
  payload->l2cap_channel_id = htole16(remote_id());
  payload->l2cap_mtu_size = htole16(max_tx_sdu_size());

  if (config->codec == hci_android::A2dpCodecType::kLdac) {
    payload->codec_information.ldac.vendor_id = htole32(config->codec_information.ldac.vendor_id);
    payload->codec_information.ldac.codec_id = htole16(config->codec_information.ldac.codec_id);
    payload->codec_information.ldac.bitrate_index = config->codec_information.ldac.bitrate_index;
    payload->codec_information.ldac.ldac_channel_mode =
        config->codec_information.ldac.ldac_channel_mode;
  } else {
    // A2dpOffloadCodecInformation does not require little endianness conversion for any other
    // codec type due to their fields being one byte only.
    payload->codec_information = config->codec_information;
  }

  a2dp_offload_status_ = A2dpOffloadStatus::kPending;
  cmd_channel_->SendCommand(
      std::move(packet),
      [cb = std::move(callback), handle = link_handle_, channel = weak_ptr_factory_.GetWeakPtr()](
          auto /*transaction_id*/, const hci::EventPacket& event) mutable {
        if (!channel) {
          return;
        }

        if (event.ToResult().is_error()) {
          bt_log(WARN, "hci", "StartA2dpOffload command failed (result: %s, handle: %#.4x)",
                 bt_str(event.ToResult()), handle);
          channel->a2dp_offload_status_ = A2dpOffloadStatus::kStopped;
        } else {
          bt_log(INFO, "hci", "A2DP offload started (handle: %#.4x", handle);
          channel->a2dp_offload_status_ = A2dpOffloadStatus::kStarted;
        }
        cb(event.ToResult());
      });
}

void ChannelImpl::OnClosed() {
  bt_log(TRACE, "l2cap", "channel closed (link: %#.4x, id: %#.4x)", link_handle(), id());

  if (!link_ || !active_) {
    link_ = nullptr;
    return;
  }

  BT_ASSERT(closed_cb_);
  auto closed_cb = std::move(closed_cb_);

  CleanUp();

  closed_cb();
}

void ChannelImpl::HandleRxPdu(PDU&& pdu) {
  TRACE_DURATION("bluetooth", "ChannelImpl::HandleRxPdu", "handle", link_->handle(), "channel_id",
                 id_);

  // link_ may be nullptr if a pdu is received after the channel has been deactivated but
  // before LogicalLink::RemoveChannel has been dispatched
  if (!link_) {
    bt_log(TRACE, "l2cap", "ignoring pdu on deactivated channel");
    return;
  }

  BT_ASSERT(rx_engine_);

  ByteBufferPtr sdu = rx_engine_->ProcessPdu(std::move(pdu));
  if (!sdu) {
    // The PDU may have been invalid, out-of-sequence, or part of a segmented
    // SDU.
    // * If invalid, we drop the PDU (per Core Spec Ver 5, Vol 3, Part A,
    //   Secs. 3.3.6 and/or 3.3.7).
    // * If out-of-sequence or part of a segmented SDU, we expect that some
    //   later call to ProcessPdu() will return us an SDU containing this
    //   PDU's data.
    return;
  }

  // Buffer the packets if the channel hasn't been activated.
  if (!active_) {
    pending_rx_sdus_.emplace(std::move(sdu));
    // Tracing: we assume pending_rx_sdus_ is only filled once and use the length of queue
    // for trace ids.
    TRACE_FLOW_BEGIN("bluetooth", "ChannelImpl::HandleRxPdu queued", pending_rx_sdus_.size());
    return;
  }

  BT_ASSERT(rx_cb_);
  {
    TRACE_DURATION("bluetooth", "ChannelImpl::HandleRxPdu callback");
    rx_cb_(std::move(sdu));
  }
}

void ChannelImpl::CleanUp() {
  RequestAclPriority(AclPriority::kNormal, [](auto result) {
    if (result.is_error()) {
      bt_log(WARN, "l2cap", "Resetting ACL priority on channel closed failed");
    }
  });

  active_ = false;
  link_ = nullptr;
  rx_cb_ = nullptr;
  closed_cb_ = nullptr;
  rx_engine_ = nullptr;
  tx_engine_ = nullptr;
}

void ChannelImpl::SendFrame(ByteBufferPtr pdu) {
  if (!link_ || !active_) {
    return;
  }

  // B-frames for Basic Mode contain only an "Information payload" (v5.0 Vol 3, Part A, Sec 3.1)
  FrameCheckSequenceOption fcs_option = info_.mode == ChannelMode::kEnhancedRetransmission
                                            ? FrameCheckSequenceOption::kIncludeFcs
                                            : FrameCheckSequenceOption::kNoFcs;

  // |link_| is expected to ignore this call and drop the packet if it has been closed.
  link_->SendFrame(remote_id_, *pdu, fcs_option, /*flushable=*/info_.flush_timeout.has_value());
}

}  // namespace internal
}  // namespace bt::l2cap
