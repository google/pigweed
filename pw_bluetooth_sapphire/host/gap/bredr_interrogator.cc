// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_interrogator.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace btlib {
namespace gap {

BrEdrInterrogator::Interrogation::Interrogation(hci::ConnectionPtr conn,
                                                ResultCallback cb)
    : conn_ptr(std::move(conn)), result_cb(std::move(cb)) {
  ZX_DEBUG_ASSERT(conn_ptr);
  ZX_DEBUG_ASSERT(result_cb);
}

BrEdrInterrogator::Interrogation::~Interrogation() {
  Finish(hci::Status(common::HostError::kFailed));
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

BrEdrInterrogator::BrEdrInterrogator(RemoteDeviceCache* cache,
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

void BrEdrInterrogator::Start(DeviceId device_id, hci::ConnectionPtr conn_ptr,
                              ResultCallback callback) {
  ZX_DEBUG_ASSERT(conn_ptr);
  ZX_DEBUG_ASSERT(callback);

  hci::ConnectionHandle handle = conn_ptr->handle();

  pending_.emplace(device_id, std::make_unique<Interrogation>(
                                  std::move(conn_ptr), std::move(callback)));

  RemoteDevice* device = cache_->FindDeviceById(device_id);
  if (!device) {
    Complete(device_id, hci::Status(common::HostError::kFailed));
    return;
  }

  if (!device->name()) {
    MakeRemoteNameRequest(device_id);
  }

  if (!device->version()) {
    ReadRemoteVersionInformation(device_id, handle);
  }

  if (!device->features().HasPage(0)) {
    ReadRemoteFeatures(device_id, handle);
  } else if (device->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
    device->set_last_page_number(1);
    ReadRemoteExtendedFeatures(device_id, handle, 1);
  }
}

void BrEdrInterrogator::Cancel(DeviceId device_id) {
  async::PostTask(dispatcher_, [id = std::move(device_id),
                                self = weak_ptr_factory_.GetWeakPtr()]() {
    if (!self) {
      return;
    }

    auto it = self->pending_.find(id);
    if (it == self->pending_.end()) {
      return;
    }

    it->second->Finish(hci::Status(common::HostError::kCanceled));
    self->pending_.erase(it);
  });
}

void BrEdrInterrogator::MaybeComplete(DeviceId device_id) {
  RemoteDevice* device = cache_->FindDeviceById(device_id);
  if (!device) {
    Complete(device_id, hci::Status(common::HostError::kFailed));
    return;
  }
  if (!device->name()) {
    return;
  }

  if (!device->version()) {
    return;
  }

  if (!device->features().HasPage(0)) {
    return;
  } else if (device->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
    for (uint8_t page = 1; page <= device->features().last_page_number();
         page++) {
      if (!device->features().HasPage(page)) {
        return;
      }
    }
  }

  Complete(device_id, hci::Status());
}

void BrEdrInterrogator::Complete(DeviceId device_id, hci::Status status) {
  auto it = pending_.find(std::move(device_id));
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second->Finish(std::move(status));
  pending_.erase(it);
}

void BrEdrInterrogator::MakeRemoteNameRequest(DeviceId device_id) {
  RemoteDevice* device = cache_->FindDeviceById(device_id);
  if (!device) {
    Complete(device_id, hci::Status(common::HostError::kFailed));
    return;
  }
  ZX_DEBUG_ASSERT(device->bredr());
  hci::PageScanRepetitionMode mode = hci::PageScanRepetitionMode::kR0;
  if (device->bredr()->page_scan_repetition_mode()) {
    mode = *device->bredr()->page_scan_repetition_mode();
  }
  auto packet = hci::CommandPacket::New(
      hci::kRemoteNameRequest, sizeof(hci::RemoteNameRequestCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto params = packet->mutable_view()
                    ->mutable_payload<hci::RemoteNameRequestCommandParams>();
  params->bd_addr = device->address().value();
  params->page_scan_repetition_mode = mode;
  if (device->bredr()->clock_offset()) {
    params->clock_offset = *(device->bredr()->clock_offset());
  }

  auto it = pending_.find(device_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second->callbacks.emplace_back([device_id,
                                      self = weak_ptr_factory_.GetWeakPtr()](
                                         auto, const auto& event) {
    if (hci_is_error(event, WARN, "gap-bredr", "remote name request failed")) {
      self->Complete(device_id, event.ToStatus());
      return;
    }

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_DEBUG_ASSERT(event.event_code() ==
                    hci::kRemoteNameRequestCompleteEventCode);

    const auto& params =
        event.view()
            .template payload<hci::RemoteNameRequestCompleteEventParams>();

    size_t len = 0;
    for (; len < hci::kMaxNameLength; len++) {
      if (params.remote_name[len] == 0) {
        break;
      }
    }
    RemoteDevice* device = self->cache_->FindDeviceById(device_id);
    if (!device) {
      self->Complete(device_id, hci::Status(common::HostError::kFailed));
      return;
    }
    device->SetName(std::string(params.remote_name, params.remote_name + len));

    self->MaybeComplete(device_id);
  });

  bt_log(SPEW, "gap-bredr", "name request %s",
         device->address().ToString().c_str());
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second->callbacks.back().callback(),
      hci::kRemoteNameRequestCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteVersionInformation(
    DeviceId device_id, hci::ConnectionHandle handle) {
  auto packet =
      hci::CommandPacket::New(hci::kReadRemoteVersionInfo,
                              sizeof(hci::ReadRemoteVersionInfoCommandParams));
  packet->mutable_view()
      ->mutable_payload<hci::ReadRemoteVersionInfoCommandParams>()
      ->connection_handle = htole16(handle);

  auto it = pending_.find(device_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second->callbacks.emplace_back([device_id,
                                      self = weak_ptr_factory_.GetWeakPtr()](
                                         auto, const auto& event) {
    if (hci_is_error(event, WARN, "gap-bredr",
                     "read remote version info failed")) {
      self->Complete(device_id, event.ToStatus());
      return;
    }

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_DEBUG_ASSERT(event.event_code() ==
                    hci::kReadRemoteVersionInfoCompleteEventCode);

    const auto params =
        event.view()
            .template payload<hci::ReadRemoteVersionInfoCompleteEventParams>();

    RemoteDevice* device = self->cache_->FindDeviceById(device_id);
    if (!device) {
      self->Complete(device_id, hci::Status(common::HostError::kFailed));
      return;
    }
    device->set_version(params.lmp_version, params.manufacturer_name,
                        params.lmp_subversion);

    self->MaybeComplete(device_id);
  });

  bt_log(SPEW, "gap-bredr", "asking for version info");
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second->callbacks.back().callback(),
      hci::kReadRemoteVersionInfoCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteFeatures(DeviceId device_id,
                                           hci::ConnectionHandle handle) {
  auto packet = hci::CommandPacket::New(
      hci::kReadRemoteSupportedFeatures,
      sizeof(hci::ReadRemoteSupportedFeaturesCommandParams));
  packet->mutable_view()
      ->mutable_payload<hci::ReadRemoteSupportedFeaturesCommandParams>()
      ->connection_handle = htole16(handle);

  auto it = pending_.find(device_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second->callbacks.emplace_back(
      [device_id, handle, self = weak_ptr_factory_.GetWeakPtr()](
          auto, const auto& event) {
        if (hci_is_error(event, WARN, "gap-bredr",
                         "read remote supported features failed")) {
          self->Complete(device_id, event.ToStatus());
          return;
        }

        if (event.event_code() == hci::kCommandStatusEventCode) {
          return;
        }

        ZX_DEBUG_ASSERT(event.event_code() ==
                        hci::kReadRemoteSupportedFeaturesCompleteEventCode);

        const auto& params =
            event.view()
                .template payload<
                    hci::ReadRemoteSupportedFeaturesCompleteEventParams>();

        RemoteDevice* device = self->cache_->FindDeviceById(device_id);
        if (!device) {
          self->Complete(device_id, hci::Status(common::HostError::kFailed));
          return;
        }
        device->SetFeaturePage(0, le64toh(params.lmp_features));

        if (device->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
          device->set_last_page_number(1);
          self->ReadRemoteExtendedFeatures(device_id, handle, 1);
        }

        self->MaybeComplete(device_id);
      });

  bt_log(SPEW, "gap-bredr", "asking for supported features");
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second->callbacks.back().callback(),
      hci::kReadRemoteSupportedFeaturesCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteExtendedFeatures(DeviceId device_id,
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

  auto it = pending_.find(device_id);
  ZX_DEBUG_ASSERT(it != pending_.end());

  it->second->callbacks.emplace_back([device_id, handle, page,
                                      self = weak_ptr_factory_.GetWeakPtr()](
                                         auto, const auto& event) {
    if (hci_is_error(event, WARN, "gap-bredr",
                     "read remote extended features failed")) {
      self->Complete(device_id, event.ToStatus());
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

    RemoteDevice* device = self->cache_->FindDeviceById(device_id);
    if (!device) {
      self->Complete(device_id, hci::Status(common::HostError::kFailed));
      return;
    }
    device->SetFeaturePage(params.page_number, le64toh(params.lmp_features));
    if (params.page_number != page) {
      bt_log(INFO, "gap-bredr",
             "requested page %u and received page %u, giving up", page,
             params.page_number);
      device->set_last_page_number(0);
    } else {
      device->set_last_page_number(params.max_page_number);
    }

    if (params.page_number < device->features().last_page_number()) {
      self->ReadRemoteExtendedFeatures(device_id, handle,
                                       params.page_number + 1);
    }
    self->MaybeComplete(device_id);
  });

  bt_log(SPEW, "gap-bredr", "get ext page %u", page);
  hci_->command_channel()->SendCommand(
      std::move(packet), dispatcher_, it->second->callbacks.back().callback(),
      hci::kReadRemoteExtendedFeaturesCompleteEventCode);
}

}  // namespace gap
}  // namespace btlib
