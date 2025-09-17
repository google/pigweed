// Copyright 2025 The Pigweed Authors
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

#pragma once

#include <fidl/fuchsia.bluetooth.le/cpp/natural_messaging.h>
#include <fidl/fuchsia.bluetooth.le/cpp/natural_types.h>

#include "pw_bluetooth_sapphire/internal/host/gap/adapter.h"

namespace bthost {

// FIDL server implementation of the PeriodicAdvertisingSync protocol. A server
// corresponds to a single periodic advertising synchronization.
class PeriodicAdvertisingSyncServer final
    : public ::fidl::Server<::fuchsia_bluetooth_le::PeriodicAdvertisingSync>,
      public bt::gap::Adapter::LowEnergy::PeriodicAdvertisingSyncDelegate {
 public:
  // @param server_end The FIDL server end this server should bind to.
  // @param peer The ID of the peer to synchronize to.
  // @param advertising_sid The SID of the advertisement to synchronize to.
  // @param options Configuration parameters for the synchronization.
  // @param closed_callback Called on asynchronous errors when synchronization
  // has been lost and the server should be destroyed.
  // @return PeriodicAdvertisingSyncServer on success, nullptr on synchronous
  // failure.
  static std::unique_ptr<PeriodicAdvertisingSyncServer> Create(
      async_dispatcher_t* dispatcher,
      fidl::ServerEnd<::fuchsia_bluetooth_le::PeriodicAdvertisingSync>
          server_end,
      bt::gap::Adapter::WeakPtr adapter,
      bt::PeerId peer,
      uint8_t advertising_sid,
      bt::gap::Adapter::LowEnergy::SyncOptions options,
      fit::callback<void()> closed_callback);

 private:
  enum class State : uint8_t { kPending, kEstablished, kError };

  PeriodicAdvertisingSyncServer(
      bt::PeerId peer_id,
      async_dispatcher_t* dispatcher,
      fidl::ServerEnd<::fuchsia_bluetooth_le::PeriodicAdvertisingSync>
          server_end,
      fit::callback<void()> closed_callback);

  // ::fidl::Server<::fuchsia_bluetooth_le::PeriodicAdvertisingSync> overrides:
  void WatchAdvertisingReport(
      WatchAdvertisingReportCompleter::Sync& completer) override;
  void SyncToSubevents(SyncToSubeventsRequest& request,
                       SyncToSubeventsCompleter::Sync& completer) override;
  void Cancel(CancelCompleter::Sync& completer) override;
  void handle_unknown_method(
      ::fidl::UnknownMethodMetadata<
          ::fuchsia_bluetooth_le::PeriodicAdvertisingSync> metadata,
      ::fidl::UnknownMethodCompleter::Sync& completer) override;

  // PeriodicAdvertisingSyncDelegate overrides:
  void OnSyncEstablished(bt::hci::SyncId id,
                         bt::gap::PeriodicAdvertisingSyncManager::SyncParameters
                             parameters) override;
  void OnSyncLost(bt::hci::SyncId id, bt::hci::Error error) override;
  void OnAdvertisingReport(
      bt::hci::SyncId id,
      const bt::gap::PeriodicAdvertisingReport& report) override;
  void OnBigInfoReport(
      bt::hci::SyncId id,
      const bt::hci_spec::BroadcastIsochronousGroupInfo& report) override;

  void OnUnbound(
      ::fidl::UnbindInfo info,
      ::fidl::ServerEnd<::fuchsia_bluetooth_le::PeriodicAdvertisingSync>
          server_end);

  void Close(zx_status_t epitaph);

  void QueueReport(::fuchsia_bluetooth_le::SyncReport&& report);

  void MaybeSendReports();

  State state_ = State::kPending;

  bt::PeerId peer_id_;

  fit::callback<void()> closed_callback_;
  std::optional<bt::gap::PeriodicAdvertisingSyncHandle> sync_handle_;

  std::optional<WatchAdvertisingReportCompleter::Async>
      watch_advertising_report_completer_;
  std::queue<::fuchsia_bluetooth_le::SyncReport> reports_;

  async_dispatcher_t* dispatcher_;

  ::fidl::ServerBindingRef<::fuchsia_bluetooth_le::PeriodicAdvertisingSync>
      binding_ref_;
};

}  // namespace bthost
