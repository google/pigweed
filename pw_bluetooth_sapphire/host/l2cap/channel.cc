// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel.h"

#include <lib/async/default.h>
#include <lib/trace/event.h>
#include <zircon/assert.h>

#include <functional>
#include <memory>

#include "lib/fitx/result.h"
#include "logical_link.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_rx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_tx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_engines.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::l2cap {

Channel::Channel(ChannelId id, ChannelId remote_id, bt::LinkType link_type,
                 hci_spec::ConnectionHandle link_handle, ChannelInfo info)
    : id_(id),
      remote_id_(remote_id),
      link_type_(link_type),
      link_handle_(link_handle),
      info_(info),
      requested_acl_priority_(hci::AclPriority::kNormal) {
  ZX_DEBUG_ASSERT(id_);
  ZX_DEBUG_ASSERT(link_type_ == bt::LinkType::kLE || link_type_ == bt::LinkType::kACL);
}

namespace internal {

namespace {

constexpr const char* kInspectLocalIdPropertyName = "local_id";
constexpr const char* kInspectRemoteIdPropertyName = "remote_id";
constexpr const char* kInspectPsmPropertyName = "psm";

}  // namespace

std::unique_ptr<ChannelImpl> ChannelImpl::CreateFixedChannel(
    ChannelId id, fxl::WeakPtr<internal::LogicalLink> link) {
  // A fixed channel's endpoints have the same local and remote identifiers.
  // Setting the ChannelInfo MTU to kMaxMTU effectively cancels any L2CAP-level MTU enforcement for
  // services which operate over fixed channels. Such services often define minimum MTU values in
  // their specification, so they are required to respect these MTUs internally by:
  //   1.) never sending packets larger than their spec-defined MTU.
  //   2.) handling inbound PDUs which are larger than their spec-defined MTU appropriately.
  return std::unique_ptr<ChannelImpl>(
      new ChannelImpl(id, id, link, ChannelInfo::MakeBasicMode(kMaxMTU, kMaxMTU)));
}

std::unique_ptr<ChannelImpl> ChannelImpl::CreateDynamicChannel(
    ChannelId id, ChannelId peer_id, fxl::WeakPtr<internal::LogicalLink> link, ChannelInfo info) {
  return std::unique_ptr<ChannelImpl>(new ChannelImpl(id, peer_id, link, info));
}

ChannelImpl::ChannelImpl(ChannelId id, ChannelId remote_id,
                         fxl::WeakPtr<internal::LogicalLink> link, ChannelInfo info)
    : Channel(id, remote_id, link->type(), link->handle(), info),
      active_(false),
      link_(link),
      weak_ptr_factory_(this) {
  ZX_ASSERT(link_);
  ZX_ASSERT_MSG(
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
  ZX_ASSERT(rx_callback);
  ZX_ASSERT(closed_callback);

  // Activating on a closed link has no effect. We also clear this on
  // deactivation to prevent a channel from being activated more than once.
  if (!link_)
    return false;

  ZX_ASSERT(!active_);
  active_ = true;
  rx_cb_ = std::move(rx_callback);
  closed_cb_ = std::move(closed_callback);

  // Route the buffered packets.
  if (!pending_rx_sdus_.empty()) {
    // Add reference to |rx_cb_| in case channel is destroyed as a result of handling an SDU.
    auto rx_cb = rx_cb_.share();
    auto pending = std::move(pending_rx_sdus_);
    ZX_ASSERT(pending_rx_sdus_.empty());
    TRACE_DURATION("bluetooth", "ChannelImpl::Activate pending drain");
    while (!pending.empty()) {
      TRACE_FLOW_END("bluetooth", "ChannelImpl::HandleRxPdu queued", pending.size());
      rx_cb(std::move(pending.front()));
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
  ZX_DEBUG_ASSERT(sdu);

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
  ZX_ASSERT(callback);

  if (!link_ || !active_) {
    bt_log(DEBUG, "l2cap", "Ignoring security request on inactive channel");
    return;
  }

  link_->UpgradeSecurity(level, std::move(callback));
}

void ChannelImpl::RequestAclPriority(hci::AclPriority priority,
                                     fit::callback<void(fitx::result<fitx::failed>)> callback) {
  if (!link_ || !active_) {
    bt_log(DEBUG, "l2cap", "Ignoring ACL priority request on inactive channel");
    callback(fitx::failed());
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
  ZX_ASSERT(link_type_ == bt::LinkType::kACL);

  // Channel may be inactive if this method is called before activation.
  if (!link_) {
    bt_log(DEBUG, "l2cap", "Ignoring %s on closed channel", __FUNCTION__);
    callback(ToResult(hci_spec::StatusCode::kCommandDisallowed));
    return;
  }

  auto cb_wrapper = [self = weak_ptr_factory_.GetWeakPtr(), cb = std::move(callback),
                     flush_timeout](auto result) mutable {
    if (!self) {
      cb(ToResult(hci_spec::StatusCode::kUnspecifiedError));
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

void ChannelImpl::OnClosed() {
  bt_log(TRACE, "l2cap", "channel closed (link: %#.4x, id: %#.4x)", link_handle(), id());

  if (!link_ || !active_) {
    link_ = nullptr;
    return;
  }

  ZX_ASSERT(closed_cb_);
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

  ZX_ASSERT(rx_engine_);

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

  ZX_ASSERT(rx_cb_);
  {
    TRACE_DURATION("bluetooth", "ChannelImpl::HandleRxPdu callback");
    rx_cb_(std::move(sdu));
  }
}

void ChannelImpl::CleanUp() {
  RequestAclPriority(hci::AclPriority::kNormal, [](auto result) {
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
