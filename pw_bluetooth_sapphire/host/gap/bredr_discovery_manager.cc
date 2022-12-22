// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_discovery_manager.h"

#include <lib/async/default.h>
#include <lib/async/time.h>
#include <lib/fit/defer.h>
#include <lib/stdcompat/functional.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/supplement_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::gap {

namespace {

template <typename EventParamType, typename ResultType>
std::unordered_set<Peer*> ProcessInquiryResult(PeerCache* cache, const hci::EventPacket& event) {
  std::unordered_set<Peer*> updated;
  bt_log(TRACE, "gap-bredr", "inquiry result received");

  const size_t event_payload_size = event.view().payload_size();
  BT_ASSERT_MSG(event_payload_size >= sizeof(EventParamType), "undersized (%zu) inquiry event",
                event_payload_size);
  size_t result_size = event_payload_size - sizeof(EventParamType);
  BT_ASSERT_MSG(result_size % sizeof(ResultType) == 0, "wrong size result (%zu %% %zu != 0)",
                result_size, sizeof(ResultType));

  const auto params_data = event.view().payload_data();
  const auto num_responses = params_data.ReadMember<&EventParamType::num_responses>();
  for (int i = 0; i < num_responses; i++) {
    const auto response = params_data.ReadMember<&EventParamType::responses>(i);
    DeviceAddress addr(DeviceAddress::Type::kBREDR, response.bd_addr);
    Peer* peer = cache->FindByAddress(addr);
    if (!peer) {
      peer = cache->NewPeer(addr, /*connectable=*/true);
    }
    BT_ASSERT(peer);

    peer->MutBrEdr().SetInquiryData(response);
    updated.insert(peer);
  }
  return updated;
}

}  // namespace

BrEdrDiscoverySession::BrEdrDiscoverySession(BrEdrDiscoveryManager::WeakPtr manager)
    : manager_(std::move(manager)) {}

BrEdrDiscoverySession::~BrEdrDiscoverySession() { manager_->RemoveDiscoverySession(this); }

void BrEdrDiscoverySession::NotifyDiscoveryResult(const Peer& peer) const {
  if (peer_found_callback_) {
    peer_found_callback_(peer);
  }
}

void BrEdrDiscoverySession::NotifyError() const {
  if (error_callback_) {
    error_callback_();
  }
}

BrEdrDiscoverableSession::BrEdrDiscoverableSession(BrEdrDiscoveryManager::WeakPtr manager)
    : manager_(std::move(manager)) {}

BrEdrDiscoverableSession::~BrEdrDiscoverableSession() { manager_->RemoveDiscoverableSession(this); }

BrEdrDiscoveryManager::BrEdrDiscoveryManager(hci::CommandChannel::WeakPtr cmd,
                                             hci_spec::InquiryMode mode, PeerCache* peer_cache)
    : cmd_(std::move(cmd)),
      dispatcher_(async_get_default_dispatcher()),
      cache_(peer_cache),
      result_handler_id_(0u),
      desired_inquiry_mode_(mode),
      current_inquiry_mode_(hci_spec::InquiryMode::kStandard),
      weak_self_(this) {
  BT_DEBUG_ASSERT(cache_);
  BT_DEBUG_ASSERT(cmd_.is_alive());
  BT_DEBUG_ASSERT(dispatcher_);

  result_handler_id_ =
      cmd_->AddEventHandler(hci_spec::kInquiryResultEventCode,
                            fit::bind_member<&BrEdrDiscoveryManager::InquiryResult>(this));
  BT_DEBUG_ASSERT(result_handler_id_);
  rssi_handler_id_ =
      cmd_->AddEventHandler(hci_spec::kInquiryResultWithRSSIEventCode,
                            cpp20::bind_front(&BrEdrDiscoveryManager::InquiryResult, this));
  BT_DEBUG_ASSERT(rssi_handler_id_);
  eir_handler_id_ =
      cmd_->AddEventHandler(hci_spec::kExtendedInquiryResultEventCode,
                            cpp20::bind_front(&BrEdrDiscoveryManager::ExtendedInquiryResult, this));
  BT_DEBUG_ASSERT(eir_handler_id_);

  // Set the Inquiry Scan Settings
  WriteInquiryScanSettings(kInquiryScanInterval, kInquiryScanWindow, /*interlaced=*/true);
}

BrEdrDiscoveryManager::~BrEdrDiscoveryManager() {
  cmd_->RemoveEventHandler(eir_handler_id_);
  cmd_->RemoveEventHandler(rssi_handler_id_);
  cmd_->RemoveEventHandler(result_handler_id_);
  InvalidateDiscoverySessions();
}

void BrEdrDiscoveryManager::RequestDiscovery(DiscoveryCallback callback) {
  BT_DEBUG_ASSERT(callback);

  bt_log(INFO, "gap-bredr", "RequestDiscovery");

  // If we're already waiting on a callback, then scanning is already starting.
  // Queue this to create a session when the scanning starts.
  if (!pending_discovery_.empty()) {
    bt_log(DEBUG, "gap-bredr", "discovery starting, add to pending");
    pending_discovery_.push(std::move(callback));
    return;
  }

  // If we're already scanning, just add a session.
  if (!discovering_.empty() || !zombie_discovering_.empty()) {
    bt_log(DEBUG, "gap-bredr", "add to active sessions");
    auto session = AddDiscoverySession();
    callback(fit::ok(), std::move(session));
    return;
  }

  pending_discovery_.push(std::move(callback));
  MaybeStartInquiry();
}

// Starts the inquiry procedure if any sessions exist or are waiting to start.
void BrEdrDiscoveryManager::MaybeStartInquiry() {
  if (pending_discovery_.empty() && discovering_.empty()) {
    bt_log(DEBUG, "gap-bredr", "no sessions, not starting inquiry");
    return;
  }

  bt_log(TRACE, "gap-bredr", "starting inquiry");

  auto self = weak_self_.GetWeakPtr();
  if (desired_inquiry_mode_ != current_inquiry_mode_) {
    auto packet = hci::CommandPacket::New(hci_spec::kWriteInquiryMode,
                                          sizeof(hci_spec::WriteInquiryModeCommandParams));
    packet->mutable_payload<hci_spec::WriteInquiryModeCommandParams>()->inquiry_mode =
        desired_inquiry_mode_;
    cmd_->SendCommand(std::move(packet),
                      [self, mode = desired_inquiry_mode_](auto /*unused*/, const auto& event) {
                        if (!self.is_alive()) {
                          return;
                        }

                        if (!hci_is_error(event, ERROR, "gap-bredr", "write inquiry mode failed")) {
                          self->current_inquiry_mode_ = mode;
                        }
                      });
  }

  auto inquiry = hci::EmbossCommandPacket::New<hci_spec::InquiryCommandWriter>(hci_spec::kInquiry);
  auto view = inquiry.view_t();
  view.lap().Write(hci_spec::InquiryAccessCode::GIAC);
  view.inquiry_length().Write(kInquiryLengthDefault);
  view.num_responses().Write(0);

  cmd_->SendExclusiveCommand(
      std::move(inquiry),
      [self](auto, const auto& event) {
        if (!self.is_alive()) {
          return;
        }
        auto status = event.ToResult();
        if (bt_is_error(status, WARN, "gap-bredr", "inquiry error")) {
          // Failure of some kind, signal error to the sessions.
          self->InvalidateDiscoverySessions();

          // Fallthrough for callback to pending sessions.
        }

        // Resolve the request if the controller sent back a Command Complete or
        // Status event.
        // TODO(fxbug.dev/1109): Make it impossible for Command Complete to happen here
        // and remove handling for it.
        if (event.event_code() == hci_spec::kCommandStatusEventCode ||
            event.event_code() == hci_spec::kCommandCompleteEventCode) {
          // Inquiry started, make sessions for our waiting callbacks.
          while (!self->pending_discovery_.empty()) {
            auto callback = std::move(self->pending_discovery_.front());
            self->pending_discovery_.pop();
            callback(status, (status.is_ok() ? self->AddDiscoverySession() : nullptr));
          }
          return;
        }

        BT_DEBUG_ASSERT(event.event_code() == hci_spec::kInquiryCompleteEventCode);
        self->zombie_discovering_.clear();

        if (bt_is_error(status, TRACE, "gap", "inquiry complete error")) {
          return;
        }

        // We've stopped scanning because we timed out.
        bt_log(TRACE, "gap-bredr", "inquiry complete, restart");
        self->MaybeStartInquiry();
      },
      hci_spec::kInquiryCompleteEventCode, {hci_spec::kRemoteNameRequest});
}

// Stops the inquiry procedure.
void BrEdrDiscoveryManager::StopInquiry() {
  BT_DEBUG_ASSERT(result_handler_id_);
  bt_log(TRACE, "gap-bredr", "cancelling inquiry");

  hci::EmbossCommandPacket inq_cancel =
      hci::EmbossCommandPacket::New<hci_spec::InquiryCancelCommandView>(hci_spec::kInquiryCancel);
  cmd_->SendCommand(std::move(inq_cancel), [](int64_t, const auto& event) {
    // Warn if the command failed.
    hci_is_error(event, WARN, "gap-bredr", "inquiry cancel failed");
  });
}

hci::CommandChannel::EventCallbackResult BrEdrDiscoveryManager::InquiryResult(
    const hci::EventPacket& event) {
  std::unordered_set<Peer*> peers;
  if (event.event_code() == hci_spec::kInquiryResultEventCode) {
    peers = ProcessInquiryResult<hci_spec::InquiryResultEventParams, hci_spec::InquiryResult>(
        cache_, event);
  } else if (event.event_code() == hci_spec::kInquiryResultWithRSSIEventCode) {
    peers = ProcessInquiryResult<hci_spec::InquiryResultWithRSSIEventParams,
                                 hci_spec::InquiryResultRSSI>(cache_, event);
  } else {
    bt_log(ERROR, "gap-bredr", "unsupported inquiry result type");
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  for (Peer* peer : peers) {
    if (!peer->name()) {
      RequestPeerName(peer->identifier());
    }
    for (const auto& session : discovering_) {
      session->NotifyDiscoveryResult(*peer);
    }
  }
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrDiscoveryManager::ExtendedInquiryResult(
    const hci::EventPacket& event) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kExtendedInquiryResultEventCode);

  bt_log(TRACE, "gap-bredr", "ExtendedInquiryResult received");
  if (event.view().payload_size() != sizeof(hci_spec::ExtendedInquiryResultEventParams)) {
    bt_log(WARN, "gap-bredr", "ignoring malformed result (%zu bytes)", event.view().payload_size());
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  const auto& result = event.params<hci_spec::ExtendedInquiryResultEventParams>();

  DeviceAddress addr(DeviceAddress::Type::kBREDR, result.bd_addr);
  Peer* peer = cache_->FindByAddress(addr);
  if (!peer) {
    peer = cache_->NewPeer(addr, /*connectable=*/true);
  }
  BT_DEBUG_ASSERT(peer);

  peer->MutBrEdr().SetInquiryData(result);

  if (!peer->name()) {
    RequestPeerName(peer->identifier());
  }
  for (const auto& session : discovering_) {
    session->NotifyDiscoveryResult(*peer);
  }
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

void BrEdrDiscoveryManager::UpdateEIRResponseData(std::string name,
                                                  hci::ResultFunction<> callback) {
  DataType name_type = DataType::kCompleteLocalName;
  size_t name_size = name.size();
  if (name.size() >= (hci_spec::kExtendedInquiryResponseMaxNameBytes)) {
    name_type = DataType::kShortenedLocalName;
    name_size = hci_spec::kExtendedInquiryResponseMaxNameBytes;
  }
  auto self = weak_self_.GetWeakPtr();
  auto eir_packet = hci::CommandPacket::New(hci_spec::kWriteExtendedInquiryResponse,
                                            sizeof(hci_spec::WriteExtendedInquiryResponseParams));
  eir_packet->mutable_payload<hci_spec::WriteExtendedInquiryResponseParams>()->fec_required = 0x00;
  auto eir_response_buf =
      MutableBufferView(eir_packet->mutable_payload<hci_spec::WriteExtendedInquiryResponseParams>()
                            ->extended_inquiry_response,
                        hci_spec::kExtendedInquiryResponseBytes);
  eir_response_buf.Fill(0);
  eir_response_buf[0] = name_size + 1;
  eir_response_buf[1] = static_cast<uint8_t>(name_type);
  eir_response_buf.mutable_view(2).Write(reinterpret_cast<const uint8_t*>(name.data()), name_size);
  self->cmd_->SendCommand(std::move(eir_packet),
                          [self, name = std::move(name), cb = std::move(callback)](
                              auto, const hci::EventPacket& event) mutable {
                            if (!hci_is_error(event, WARN, "gap", "write EIR failed")) {
                              self->local_name_ = std::move(name);
                            }
                            cb(event.ToResult());
                          });
}

void BrEdrDiscoveryManager::UpdateLocalName(std::string name, hci::ResultFunction<> callback) {
  auto self = weak_self_.GetWeakPtr();

  auto write_name = hci::EmbossCommandPacket::New<hci_spec::WriteLocalNameCommandWriter>(
      hci_spec::kWriteLocalName);
  auto write_name_view = write_name.view_t();
  auto local_name = write_name_view.local_name().BackingStorage();
  size_t name_size = std::min(name.size(), hci_spec::kMaxNameLength);

  // Use ContiguousBuffer instead of constructing LocalName view in case of invalid view being
  // created when name is not large enough for the view
  auto name_buf = emboss::support::ReadOnlyContiguousBuffer(&name);

  local_name.CopyFrom(name_buf, name_size);
  cmd_->SendCommand(std::move(write_name), [self, name = std::move(name), cb = std::move(callback)](
                                               auto, const hci::EventPacket& event) mutable {
    if (hci_is_error(event, WARN, "gap", "set local name failed")) {
      cb(event.ToResult());
      return;
    }
    // If the WriteLocalName command was successful, update the extended inquiry data.
    self->UpdateEIRResponseData(std::move(name), std::move(cb));
  });
}

void BrEdrDiscoveryManager::AttachInspect(inspect::Node& parent, std::string name) {
  auto node = parent.CreateChild(name);
  inspect_properties_.Initialize(std::move(node));
  UpdateInspectProperties();
}

void BrEdrDiscoveryManager::InspectProperties::Initialize(inspect::Node new_node) {
  discoverable_sessions = new_node.CreateUint("discoverable_sessions", 0);
  pending_discoverable_sessions = new_node.CreateUint("pending_discoverable", 0);
  discoverable_sessions_count = new_node.CreateUint("discoverable_sessions_count", 0);
  last_discoverable_length_sec = new_node.CreateUint("last_discoverable_length_sec", 0);

  discovery_sessions = new_node.CreateUint("discovery_sessions", 0);
  last_inquiry_length_sec = new_node.CreateUint("last_inquiry_length_sec", 0);
  inquiry_sessions_count = new_node.CreateUint("inquiry_sessions_count", 0);

  discoverable_started_time.reset();
  inquiry_started_time.reset();

  node = std::move(new_node);
}

void BrEdrDiscoveryManager::InspectProperties::Update(size_t discoverable_count,
                                                      size_t pending_discoverable_count,
                                                      size_t discovery_count, zx_time_t now) {
  if (!node) {
    return;
  }

  if (!discoverable_started_time.has_value() && discoverable_count != 0) {
    discoverable_started_time.emplace(now);
  } else if (discoverable_started_time.has_value() && discoverable_count == 0) {
    discoverable_sessions_count.Add(1);
    zx_duration_t length = now - discoverable_started_time.value();
    last_discoverable_length_sec.Set(length / zx_duration_from_sec(1));
    discoverable_started_time.reset();
  }

  if (!inquiry_started_time.has_value() && discovery_count != 0) {
    inquiry_started_time.emplace(now);
  } else if (inquiry_started_time.has_value() && discovery_count == 0) {
    inquiry_sessions_count.Add(1);
    zx_duration_t length = now - inquiry_started_time.value();
    last_inquiry_length_sec.Set(length / zx_duration_from_sec(1));
    inquiry_started_time.reset();
  }

  discoverable_sessions.Set(discoverable_count);
  pending_discoverable_sessions.Set(pending_discoverable_count);
  discovery_sessions.Set(discovery_count);
}

void BrEdrDiscoveryManager::UpdateInspectProperties() {
  inspect_properties_.Update(discoverable_.size(), pending_discoverable_.size(),
                             discovering_.size(), async_now(dispatcher_));
}

void BrEdrDiscoveryManager::RequestPeerName(PeerId id) {
  if (requesting_names_.count(id)) {
    bt_log(TRACE, "gap-bredr", "already requesting name for %s", bt_str(id));
    return;
  }
  Peer* peer = cache_->FindById(id);
  if (!peer) {
    bt_log(WARN, "gap-bredr", "cannot request name, unknown peer: %s", bt_str(id));
    return;
  }
  auto packet = hci::EmbossCommandPacket::New<hci_spec::RemoteNameRequestCommandWriter>(
      hci_spec::kRemoteNameRequest);
  auto params = packet.view_t();
  BT_DEBUG_ASSERT(peer->bredr());
  BT_DEBUG_ASSERT(peer->bredr()->page_scan_repetition_mode());
  params.bd_addr().CopyFrom(peer->address().value().view());
  params.page_scan_repetition_mode().Write(*(peer->bredr()->page_scan_repetition_mode()));
  if (peer->bredr()->clock_offset()) {
    params.clock_offset().valid().Write(true);
    uint16_t offset = peer->bredr()->clock_offset().value();
    params.clock_offset().clock_offset().Write(offset);
  }

  auto cb = [id, self = weak_self_.GetWeakPtr()](auto, const auto& event) {
    if (!self.is_alive()) {
      return;
    }
    if (hci_is_error(event, TRACE, "gap-bredr", "remote name request failed")) {
      self->requesting_names_.erase(id);
      return;
    }

    if (event.event_code() == hci_spec::kCommandStatusEventCode) {
      return;
    }

    BT_DEBUG_ASSERT(event.event_code() == hci_spec::kRemoteNameRequestCompleteEventCode);

    self->requesting_names_.erase(id);
    const auto& params =
        event.view().template payload<hci_spec::RemoteNameRequestCompleteEventParams>();
    Peer* const peer = self->cache_->FindById(id);
    if (!peer) {
      return;
    }
    const auto remote_name_end = std::find(params.remote_name, std::end(params.remote_name), '\0');
    peer->RegisterName(std::string(params.remote_name, remote_name_end),
                       Peer::NameSource::kNameDiscoveryProcedure);
  };

  auto cmd_id = cmd_->SendExclusiveCommand(std::move(packet), std::move(cb),
                                           hci_spec::kRemoteNameRequestCompleteEventCode,
                                           {hci_spec::kInquiry});
  if (cmd_id) {
    requesting_names_.insert(id);
  }
}

void BrEdrDiscoveryManager::RequestDiscoverable(DiscoverableCallback callback) {
  BT_DEBUG_ASSERT(callback);

  auto self = weak_self_.GetWeakPtr();
  auto result_cb = [self, cb = callback.share()](const hci::Result<>& result) {
    cb(result, (result.is_ok() ? self->AddDiscoverableSession() : nullptr));
  };

  auto update_inspect = fit::defer([self]() { self->UpdateInspectProperties(); });

  if (!pending_discoverable_.empty()) {
    pending_discoverable_.push(std::move(result_cb));
    bt_log(INFO, "gap-bredr", "discoverable mode starting: %lu pending",
           pending_discoverable_.size());
    return;
  }

  // If we're already discoverable, just add a session.
  if (!discoverable_.empty()) {
    result_cb(fit::ok());
    return;
  }

  pending_discoverable_.push(std::move(result_cb));
  SetInquiryScan();
}

void BrEdrDiscoveryManager::SetInquiryScan() {
  bool enable = !discoverable_.empty() || !pending_discoverable_.empty();
  bt_log(INFO, "gap-bredr", "%sabling inquiry scan: %lu sessions, %lu pending",
         (enable ? "en" : "dis"), discoverable_.size(), pending_discoverable_.size());

  auto self = weak_self_.GetWeakPtr();
  auto scan_enable_cb = [self](auto, const hci::EventPacket& event) {
    if (!self.is_alive()) {
      return;
    }

    auto status = event.ToResult();
    auto resolve_pending = fit::defer([self, &status]() {
      while (!self->pending_discoverable_.empty()) {
        auto cb = std::move(self->pending_discoverable_.front());
        self->pending_discoverable_.pop();
        cb(status);
      }
    });

    if (bt_is_error(status, WARN, "gap-bredr", "read scan enable failed")) {
      return;
    }

    bool enable = !self->discoverable_.empty() || !self->pending_discoverable_.empty();
    auto params = event.return_params<hci_spec::ReadScanEnableReturnParams>();
    uint8_t scan_type = params->scan_enable;
    bool enabled = scan_type & static_cast<uint8_t>(hci_spec::ScanEnableBit::kInquiry);

    if (enable == enabled) {
      bt_log(INFO, "gap-bredr", "inquiry scan already %s", (enable ? "enabled" : "disabled"));
      return;
    }

    if (enable) {
      scan_type |= static_cast<uint8_t>(hci_spec::ScanEnableBit::kInquiry);
    } else {
      scan_type &= ~static_cast<uint8_t>(hci_spec::ScanEnableBit::kInquiry);
    }

    auto write_enable = hci::EmbossCommandPacket::New<hci_spec::WriteScanEnableCommandWriter>(
        hci_spec::kWriteScanEnable);
    auto write_enable_view = write_enable.view_t();
    write_enable_view.scan_enable().inquiry().Write(
        scan_type & static_cast<uint8_t>(hci_spec::ScanEnableBit::kInquiry));
    write_enable_view.scan_enable().page().Write(
        scan_type & static_cast<uint8_t>(hci_spec::ScanEnableBit::kPage));
    resolve_pending.cancel();
    self->cmd_->SendCommand(std::move(write_enable), [self](auto, const hci::EventPacket& event) {
      if (!self.is_alive()) {
        return;
      }

      // Warn if the command failed
      hci_is_error(event, WARN, "gap-bredr", "write scan enable failed");

      while (!self->pending_discoverable_.empty()) {
        auto cb = std::move(self->pending_discoverable_.front());
        self->pending_discoverable_.pop();
        cb(event.ToResult());
      }
      self->UpdateInspectProperties();
    });
  };

  auto read_enable = hci::CommandPacket::New(hci_spec::kReadScanEnable);
  cmd_->SendCommand(std::move(read_enable), std::move(scan_enable_cb));
}

void BrEdrDiscoveryManager::WriteInquiryScanSettings(uint16_t interval, uint16_t window,
                                                     bool interlaced) {
  // TODO(jamuraa): add a callback for success or failure?
  auto write_activity = hci::CommandPacket::New(
      hci_spec::kWriteInquiryScanActivity, sizeof(hci_spec::WriteInquiryScanActivityCommandParams));
  auto* activity_params =
      write_activity->mutable_payload<hci_spec::WriteInquiryScanActivityCommandParams>();
  activity_params->inquiry_scan_interval = htole16(interval);
  activity_params->inquiry_scan_window = htole16(window);

  cmd_->SendCommand(std::move(write_activity), [](auto id, const hci::EventPacket& event) {
    if (hci_is_error(event, WARN, "gap-bredr", "write inquiry scan activity failed")) {
      return;
    }
    bt_log(TRACE, "gap-bredr", "inquiry scan activity updated");
  });

  auto write_type = hci::CommandPacket::New(hci_spec::kWriteInquiryScanType,
                                            sizeof(hci_spec::WriteInquiryScanTypeCommandParams));
  auto* type_params = write_type->mutable_payload<hci_spec::WriteInquiryScanTypeCommandParams>();
  type_params->inquiry_scan_type = (interlaced ? hci_spec::InquiryScanType::kInterlacedScan
                                               : hci_spec::InquiryScanType::kStandardScan);

  cmd_->SendCommand(std::move(write_type), [](auto id, const hci::EventPacket& event) {
    if (hci_is_error(event, WARN, "gap-bredr", "write inquiry scan type failed")) {
      return;
    }
    bt_log(TRACE, "gap-bredr", "inquiry scan type updated");
  });
}

std::unique_ptr<BrEdrDiscoverySession> BrEdrDiscoveryManager::AddDiscoverySession() {
  bt_log(TRACE, "gap-bredr", "adding discovery session");

  // Cannot use make_unique here since BrEdrDiscoverySession has a private
  // constructor.
  std::unique_ptr<BrEdrDiscoverySession> session(
      new BrEdrDiscoverySession(weak_self_.GetWeakPtr()));
  BT_DEBUG_ASSERT(discovering_.find(session.get()) == discovering_.end());
  discovering_.insert(session.get());
  bt_log(INFO, "gap-bredr", "new discovery session: %lu sessions active", discovering_.size());
  UpdateInspectProperties();
  return session;
}

void BrEdrDiscoveryManager::RemoveDiscoverySession(BrEdrDiscoverySession* session) {
  bt_log(TRACE, "gap-bredr", "removing discovery session");

  auto removed = discovering_.erase(session);
  // TODO(fxbug.dev/668): Cancel the running inquiry with StopInquiry() instead.
  if (removed) {
    zombie_discovering_.insert(session);
  }
  UpdateInspectProperties();
}

std::unique_ptr<BrEdrDiscoverableSession> BrEdrDiscoveryManager::AddDiscoverableSession() {
  bt_log(TRACE, "gap-bredr", "adding discoverable session");

  // Cannot use make_unique here since BrEdrDiscoverableSession has a private
  // constructor.
  std::unique_ptr<BrEdrDiscoverableSession> session(
      new BrEdrDiscoverableSession(weak_self_.GetWeakPtr()));
  BT_DEBUG_ASSERT(discoverable_.find(session.get()) == discoverable_.end());
  discoverable_.insert(session.get());
  bt_log(INFO, "gap-bredr", "new discoverable session: %lu sessions active", discoverable_.size());
  return session;
}

void BrEdrDiscoveryManager::RemoveDiscoverableSession(BrEdrDiscoverableSession* session) {
  bt_log(DEBUG, "gap-bredr", "removing discoverable session");
  discoverable_.erase(session);
  if (discoverable_.empty()) {
    SetInquiryScan();
  }
  UpdateInspectProperties();
}

void BrEdrDiscoveryManager::InvalidateDiscoverySessions() {
  for (auto session : discovering_) {
    session->NotifyError();
  }
  discovering_.clear();
  UpdateInspectProperties();
}

}  // namespace bt::gap
