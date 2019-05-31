// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_interrogator.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace bt {
namespace gap {

BrEdrInterrogator::Interrogation::Interrogation(hci::ConnectionPtr conn,
                                                ResultCallback cb)
    : conn_ptr(std::move(conn)), result_cb(std::move(cb)) {
  ZX_DEBUG_ASSERT(conn_ptr);
  ZX_DEBUG_ASSERT(result_cb);
}

BrEdrInterrogator::Interrogation::~Interrogation() {
  Finish(hci::Status(HostError::kFailed));
}

void BrEdrInterrogator::Interrogation::Finish(hci::Status status) {
  // If the connection is gone, we are finished already.
  if (!conn_ptr) {
    return;
  }
  // Cancel any callbacks we might receive.
  callbacks.clear();

  result_cb(status, std::move(conn_ptr));
}

BrEdrInterrogator::BrEdrInterrogator(PeerCache* cache,
                                     fxl::RefPtr<hci::Transport> hci,
                                     async_dispatcher_t* dispatcher)
    : hci_(hci),
      dispatcher_(dispatcher),
      cache_(cache),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(dispatcher_);
  ZX_DEBUG_ASSERT(cache_);
}

BrEdrInterrogator::~BrEdrInterrogator() {
  for (auto& p : pending_) {
    Cancel(p.first);
  }
}

void BrEdrInterrogator::Start(PeerId peer_id, hci::ConnectionPtr conn_ptr,
                              ResultCallback callback) {
  ZX_DEBUG_ASSERT(conn_ptr);
  ZX_DEBUG_ASSERT(callback);

  hci::ConnectionHandle handle = conn_ptr->handle();

  auto placed =
      pending_.try_emplace(peer_id, std::move(conn_ptr), std::move(callback));
  ZX_DEBUG_ASSERT_MSG(placed.second, "interrogating peer %s twice at once",
                      bt_str(peer_id));

  Peer* peer = cache_->FindById(peer_id);
  if (!peer) {
    Complete(peer_id, hci::Status(HostError::kFailed));
    return;
  }

  if (!peer->name()) {
    MakeRemoteNameRequest(peer_id);
  }

  if (!peer->version()) {
    ReadRemoteVersionInformation(peer_id, handle);
  }

  if (!peer->features().HasPage(0)) {
    ReadRemoteFeatures(peer_id, handle);
  } else if (peer->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
    peer->set_last_page_number(1);
    ReadRemoteExtendedFeatures(peer_id, handle, 1);
  } else {
    // Test completion if we didn't request remote features.
    MaybeComplete(peer_id);
  }
}

void BrEdrInterrogator::Cancel(PeerId peer_id) {
  async::PostTask(dispatcher_,
                  [peer_id, self = weak_ptr_factory_.GetWeakPtr()]() {
                    if (!self) {
                      return;
                    }

                    auto node = self->pending_.extract(peer_id);
                    if (!node) {
                      return;
                    }

                    node.mapped().Finish(hci::Status(HostError::kCanceled));
                  });
}

void BrEdrInterrogator::MaybeComplete(PeerId peer_id) {
  Peer* peer = cache_->FindById(peer_id);
  if (!peer) {
    Complete(peer_id, hci::Status(HostError::kFailed));
    return;
  }
  if (!peer->name()) {
    return;
  }
  if (!peer->version()) {
    return;
  }
  if (!peer->features().HasPage(0)) {
    return;
  } else if (peer->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
    for (uint8_t page = 1; page <= peer->features().last_page_number();
         page++) {
      if (!peer->features().HasPage(page)) {
        return;
      }
    }
  }

  Complete(peer_id, hci::Status());
}

void BrEdrInterrogator::Complete(PeerId peer_id, hci::Status status) {
  auto node = pending_.extract(peer_id);
  ZX_DEBUG_ASSERT(node);

  node.mapped().Finish(std::move(status));
}

void BrEdrInterrogator::MakeRemoteNameRequest(PeerId peer_id) {
  Peer* peer = cache_->FindById(peer_id);
  if (!peer) {
    Complete(peer_id, hci::Status(HostError::kFailed));
    return;
  }
  ZX_DEBUG_ASSERT(peer->bredr());
  hci::PageScanRepetitionMode mode = hci::PageScanRepetitionMode::kR0;
  if (peer->bredr()->page_scan_repetition_mode()) {
    mode = *peer->bredr()->page_scan_repetition_mode();
  }
  auto packet = hci::CommandPacket::New(
      hci::kRemoteNameRequest, sizeof(hci::RemoteNameRequestCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto params = packet->mutable_view()
                    ->mutable_payload<hci::RemoteNameRequestCommandParams>();
  params->bd_addr = peer->address().value();
  params->page_scan_repetition_mode = mode;
  if (peer->bredr()->clock_offset()) {
    params->clock_offset = *(peer->bredr()->clock_offset());
  }

  auto it = pending_.find(peer_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second.callbacks.emplace_back([peer_id,
                                     self = weak_ptr_factory_.GetWeakPtr()](
                                        auto, const hci::EventPacket& event) {
    if (hci_is_error(event, WARN, "gap-bredr", "remote name request failed")) {
      self->Complete(peer_id, event.ToStatus());
      return;
    }

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_DEBUG_ASSERT(event.event_code() ==
                    hci::kRemoteNameRequestCompleteEventCode);

    const auto& params =
        event.params<hci::RemoteNameRequestCompleteEventParams>();

    size_t len = 0;
    for (; len < hci::kMaxNameLength; len++) {
      if (params.remote_name[len] == 0) {
        break;
      }
    }
    Peer* peer = self->cache_->FindById(peer_id);
    if (!peer) {
      self->Complete(peer_id, hci::Status(HostError::kFailed));
      return;
    }
    peer->SetName(std::string(params.remote_name, params.remote_name + len));

    self->MaybeComplete(peer_id);
  });

  bt_log(SPEW, "gap-bredr", "name request %s", bt_str(peer->address()));
  hci_->command_channel()->SendExclusiveCommand(
      std::move(packet), dispatcher_, it->second.callbacks.back().callback(),
      hci::kRemoteNameRequestCompleteEventCode, {hci::kInquiry});
}

void BrEdrInterrogator::ReadRemoteVersionInformation(
    PeerId peer_id, hci::ConnectionHandle handle) {
  auto packet =
      hci::CommandPacket::New(hci::kReadRemoteVersionInfo,
                              sizeof(hci::ReadRemoteVersionInfoCommandParams));
  packet->mutable_view()
      ->mutable_payload<hci::ReadRemoteVersionInfoCommandParams>()
      ->connection_handle = htole16(handle);

  auto it = pending_.find(peer_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second.callbacks.emplace_back(
      [peer_id, self = weak_ptr_factory_.GetWeakPtr()](
          auto, const hci::EventPacket& event) {
        if (hci_is_error(event, WARN, "gap-bredr",
                         "read remote version info failed")) {
          self->Complete(peer_id, event.ToStatus());
          return;
        }

        if (event.event_code() == hci::kCommandStatusEventCode) {
          return;
        }

        ZX_DEBUG_ASSERT(event.event_code() ==
                        hci::kReadRemoteVersionInfoCompleteEventCode);

        const auto params =
            event.params<hci::ReadRemoteVersionInfoCompleteEventParams>();

        Peer* peer = self->cache_->FindById(peer_id);
        if (!peer) {
          self->Complete(peer_id, hci::Status(HostError::kFailed));
          return;
        }
        peer->set_version(params.lmp_version, params.manufacturer_name,
                          params.lmp_subversion);

        self->MaybeComplete(peer_id);
      });

  bt_log(SPEW, "gap-bredr", "asking for version info");
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second.callbacks.back().callback(),
      hci::kReadRemoteVersionInfoCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteFeatures(PeerId peer_id,
                                           hci::ConnectionHandle handle) {
  auto packet = hci::CommandPacket::New(
      hci::kReadRemoteSupportedFeatures,
      sizeof(hci::ReadRemoteSupportedFeaturesCommandParams));
  packet->mutable_view()
      ->mutable_payload<hci::ReadRemoteSupportedFeaturesCommandParams>()
      ->connection_handle = htole16(handle);

  auto it = pending_.find(peer_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second.callbacks.emplace_back(
      [peer_id, handle, self = weak_ptr_factory_.GetWeakPtr()](
          auto, const hci::EventPacket& event) {
        if (hci_is_error(event, WARN, "gap-bredr",
                         "read remote supported features failed")) {
          self->Complete(peer_id, event.ToStatus());
          return;
        }

        if (event.event_code() == hci::kCommandStatusEventCode) {
          return;
        }

        ZX_DEBUG_ASSERT(event.event_code() ==
                        hci::kReadRemoteSupportedFeaturesCompleteEventCode);

        const auto& params =
            event.view()
                .payload<hci::ReadRemoteSupportedFeaturesCompleteEventParams>();

        Peer* peer = self->cache_->FindById(peer_id);
        if (!peer) {
          self->Complete(peer_id, hci::Status(HostError::kFailed));
          return;
        }
        peer->SetFeaturePage(0, le64toh(params.lmp_features));

        if (peer->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
          peer->set_last_page_number(1);
          self->ReadRemoteExtendedFeatures(peer_id, handle, 1);
        }

        self->MaybeComplete(peer_id);
      });

  bt_log(SPEW, "gap-bredr", "asking for supported features");
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second.callbacks.back().callback(),
      hci::kReadRemoteSupportedFeaturesCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteExtendedFeatures(PeerId peer_id,
                                                   hci::ConnectionHandle handle,
                                                   uint8_t page) {
  auto packet = hci::CommandPacket::New(
      hci::kReadRemoteExtendedFeatures,
      sizeof(hci::ReadRemoteExtendedFeaturesCommandParams));
  auto params =
      packet->mutable_view()
          ->mutable_payload<hci::ReadRemoteExtendedFeaturesCommandParams>();
  params->connection_handle = htole16(handle);
  params->page_number = page;

  auto it = pending_.find(peer_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second.callbacks.emplace_back([peer_id, handle, page,
                                     self = weak_ptr_factory_.GetWeakPtr()](
                                        auto, const auto& event) {
    if (hci_is_error(event, WARN, "gap-bredr",
                     "read remote extended features failed")) {
      self->Complete(peer_id, event.ToStatus());
      return;
    }

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_DEBUG_ASSERT(event.event_code() ==
                    hci::kReadRemoteExtendedFeaturesCompleteEventCode);

    const auto& params =
        event.view()
            .template payload<
                hci::ReadRemoteExtendedFeaturesCompleteEventParams>();

    Peer* peer = self->cache_->FindById(peer_id);
    if (!peer) {
      self->Complete(peer_id, hci::Status(HostError::kFailed));
      return;
    }
    peer->SetFeaturePage(params.page_number, le64toh(params.lmp_features));
    if (params.page_number != page) {
      bt_log(INFO, "gap-bredr", "requested page %u and got page %u, giving up",
             page, params.page_number);
      peer->set_last_page_number(0);
    } else {
      peer->set_last_page_number(params.max_page_number);
    }

    if (params.page_number < peer->features().last_page_number()) {
      self->ReadRemoteExtendedFeatures(peer_id, handle, params.page_number + 1);
    }
    self->MaybeComplete(peer_id);
  });

  bt_log(SPEW, "gap-bredr", "get ext page %u", page);
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second.callbacks.back().callback(),
      hci::kReadRemoteExtendedFeaturesCompleteEventCode);
}

}  // namespace gap
}  // namespace bt
