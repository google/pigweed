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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/periodic_advertising_sync_server.h"

#include <lib/async/cpp/time.h>

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/helpers.h"

namespace fble = ::fuchsia_bluetooth_le;

namespace bthost {

namespace {

constexpr size_t kMaxQueuedReports = 10;

::fuchsia_bluetooth_le::PeriodicAdvertisingSyncOnEstablishedRequest
OnEstablishedRequestFrom(
    bt::PeerId peer_id,
    bt::hci::SyncId sync_id,
    const bt::gap::PeriodicAdvertisingSyncManager::SyncParameters& params) {
  ::fuchsia_bluetooth_le::PeriodicAdvertisingSyncOnEstablishedRequest req;
  req.id() = sync_id.value();
  req.subevents_count() = params.subevents_count;
  req.peer_id() = ::fuchsia_bluetooth::PeerId{peer_id.value()};
  // TODO: https://fxbug.dev/309014342 - Set service data when PAST is
  // implemented.
  req.advertising_sid() = params.advertising_sid;
  req.phy() = fidl_helpers::LEPhyToFidl(params.phy);
  req.periodic_advertising_interval() = params.interval;
  return req;
}

}  // namespace

std::unique_ptr<PeriodicAdvertisingSyncServer>
PeriodicAdvertisingSyncServer::Create(
    async_dispatcher_t* dispatcher,
    fidl::ServerEnd<::fuchsia_bluetooth_le::PeriodicAdvertisingSync> server_end,
    bt::gap::Adapter::WeakPtr adapter,
    bt::PeerId peer,
    uint8_t advertising_sid,
    bt::gap::Adapter::LowEnergy::SyncOptions options,
    fit::callback<void()> closed_callback) {
  std::unique_ptr<PeriodicAdvertisingSyncServer> server(
      new PeriodicAdvertisingSyncServer(
          peer, dispatcher, std::move(server_end), std::move(closed_callback)));

  bt::hci::Result<bt::gap::PeriodicAdvertisingSyncHandle> result =
      adapter->le()->SyncToPeriodicAdvertisement(
          peer, advertising_sid, options, *server);

  if (result.is_error()) {
    server->state_ = State::kError;

    bt_log(WARN,
           "fidl",
           "SyncToPeriodicAdvertisement failed: %s",
           bt_str(result.error_value()));

    ::fit::result<::fidl::OneWayError> status =
        ::fidl::SendEvent(server->binding_ref_)
            ->OnError(fble::PeriodicAdvertisingSyncError::kNotSupportedLocal);

    if (status.is_error()) {
      bt_log(INFO,
             "fidl",
             "failed to send PeriodicAdvertisingSync.OnError: %s",
             status.error_value().status_string());
    }

    return nullptr;
  }

  server->sync_handle_ = std::move(result.value());

  return server;
}

PeriodicAdvertisingSyncServer::PeriodicAdvertisingSyncServer(
    bt::PeerId peer_id,
    async_dispatcher_t* dispatcher,
    fidl::ServerEnd<::fuchsia_bluetooth_le::PeriodicAdvertisingSync> server_end,
    fit::callback<void()> closed_callback)
    : peer_id_(peer_id),
      closed_callback_(std::move(closed_callback)),
      dispatcher_(dispatcher),
      binding_ref_(::fidl::BindServer(
          dispatcher,
          std::move(server_end),
          this,
          std::mem_fn(&PeriodicAdvertisingSyncServer::OnUnbound))) {}

void PeriodicAdvertisingSyncServer::OnUnbound(
    ::fidl::UnbindInfo info,
    ::fidl::ServerEnd<::fuchsia_bluetooth_le::PeriodicAdvertisingSync>
        server_end) {
  sync_handle_.reset();
  Close(ZX_ERR_CANCELED);
}

void PeriodicAdvertisingSyncServer::WatchAdvertisingReport(
    WatchAdvertisingReportCompleter::Sync& completer) {
  if (watch_advertising_report_completer_.has_value()) {
    Close(ZX_ERR_BAD_STATE);
    return;
  }
  watch_advertising_report_completer_ = completer.ToAsync();
  MaybeSendReports();
}

void PeriodicAdvertisingSyncServer::SyncToSubevents(
    SyncToSubeventsRequest& request,
    SyncToSubeventsCompleter::Sync& completer) {
  ::fidl::Response<fble::PeriodicAdvertisingSync::SyncToSubevents> response(
      fit::error(ZX_ERR_NOT_SUPPORTED));
  completer.Reply(response);
}

void PeriodicAdvertisingSyncServer::Cancel(CancelCompleter::Sync& completer) {
  sync_handle_.reset();
  Close(ZX_ERR_CANCELED);
}

void PeriodicAdvertisingSyncServer::handle_unknown_method(
    ::fidl::UnknownMethodMetadata<
        ::fuchsia_bluetooth_le::PeriodicAdvertisingSync> metadata,
    ::fidl::UnknownMethodCompleter::Sync& completer) {
  bt_log(WARN,
         "fidl",
         "received unknown method with ordinal: %" PRIu64,
         metadata.method_ordinal);
}

void PeriodicAdvertisingSyncServer::OnSyncEstablished(
    bt::hci::SyncId sync_id,
    bt::gap::PeriodicAdvertisingSyncManager::SyncParameters parameters) {
  state_ = State::kEstablished;

  ::fuchsia_bluetooth_le::PeriodicAdvertisingSyncOnEstablishedRequest request =
      OnEstablishedRequestFrom(peer_id_, sync_id, parameters);

  ::fit::result<::fidl::OneWayError> status =
      ::fidl::SendEvent(binding_ref_)->OnEstablished(request);

  if (status.is_error()) {
    bt_log(INFO,
           "fidl",
           "failed to send PeriodicAdvertisingSync.OnSyncEstablished: %s",
           status.error_value().status_string());
  }
}

void PeriodicAdvertisingSyncServer::OnSyncLost(bt::hci::SyncId id,
                                               bt::hci::Error error) {
  fble::PeriodicAdvertisingSyncError fidl_error =
      fble::PeriodicAdvertisingSyncError::kSynchronizationLost;
  if (state_ == State::kPending) {
    fidl_error =
        fble::PeriodicAdvertisingSyncError::kInitialSynchronizationFailed;
  }

  state_ = State::kError;

  ::fit::result<::fidl::OneWayError> status =
      ::fidl::SendEvent(binding_ref_)->OnError(fidl_error);

  if (status.is_error()) {
    bt_log(INFO,
           "fidl",
           "failed to send PeriodicAdvertisingSync.OnError: %s",
           status.error_value().status_string());
  }

  sync_handle_.reset();
  Close(ZX_ERR_TIMED_OUT);
}

void PeriodicAdvertisingSyncServer::OnAdvertisingReport(
    bt::hci::SyncId id, const bt::gap::PeriodicAdvertisingReport& report) {
  QueueReport(fidl_helpers::ReportFrom(report, async::Now(dispatcher_)));
  MaybeSendReports();
}

void PeriodicAdvertisingSyncServer::OnBigInfoReport(
    bt::hci::SyncId id,
    const bt::hci_spec::BroadcastIsochronousGroupInfo& report) {
  QueueReport(fidl_helpers::ReportFrom(report, async::Now(dispatcher_)));
  MaybeSendReports();
}

void PeriodicAdvertisingSyncServer::Close(zx_status_t epitaph) {
  binding_ref_.Close(epitaph);
  if (closed_callback_) {
    closed_callback_();
  }
}

void PeriodicAdvertisingSyncServer::QueueReport(
    ::fuchsia_bluetooth_le::SyncReport&& report) {
  if (reports_.size() == kMaxQueuedReports) {
    reports_.pop();
  }
  reports_.emplace(std::move(report));
}

void PeriodicAdvertisingSyncServer::MaybeSendReports() {
  if (reports_.empty() || !watch_advertising_report_completer_.has_value()) {
    return;
  }

  std::vector<::fuchsia_bluetooth_le::SyncReport> reports;
  reports.reserve(reports_.size());
  while (!reports_.empty()) {
    reports.emplace_back(std::move(reports_.front()));
    reports_.pop();
  }

  ::fidl::Response<
      ::fuchsia_bluetooth_le::PeriodicAdvertisingSync::WatchAdvertisingReport>
      response;
  response.reports() = std::move(reports);
  watch_advertising_report_completer_->Reply(response);
  watch_advertising_report_completer_.reset();
}

}  // namespace bthost
