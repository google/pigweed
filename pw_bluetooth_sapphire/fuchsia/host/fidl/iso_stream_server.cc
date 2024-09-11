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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/iso_stream_server.h"

#include <lib/fidl/cpp/wire/channel.h>

#include <cinttypes>

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/helpers.h"

namespace bthost {

IsoStreamServer::IsoStreamServer(
    fidl::InterfaceRequest<fuchsia::bluetooth::le::IsochronousStream> request,
    fit::callback<void()> on_closed_cb)
    : ServerBase(this, std::move(request)),
      on_closed_cb_(std::move(on_closed_cb)),
      weak_self_(this) {
  set_error_handler([this](zx_status_t) { OnClosed(); });
}

void IsoStreamServer::OnStreamEstablished(
    bt::iso::IsoStream::WeakPtr stream_ptr,
    const bt::iso::CisEstablishedParameters& connection_params) {
  bt_log(INFO, "fidl", "CIS established");
  iso_stream_ = stream_ptr;
  fuchsia::bluetooth::le::IsochronousStreamOnEstablishedRequest request;
  request.set_result(ZX_OK);
  fuchsia::bluetooth::le::CisEstablishedParameters params =
      bthost::fidl_helpers::CisEstablishedParametersToFidl(connection_params);
  request.set_established_params(std::move(params));
  binding()->events().OnEstablished(std::move(request));
}

void IsoStreamServer::OnStreamEstablishmentFailed(
    pw::bluetooth::emboss::StatusCode status) {
  BT_ASSERT(status != pw::bluetooth::emboss::StatusCode::SUCCESS);
  bt_log(WARN,
         "fidl",
         "CIS failed to be established: %u",
         static_cast<unsigned>(status));
  fuchsia::bluetooth::le::IsochronousStreamOnEstablishedRequest request;
  request.set_result(ZX_ERR_INTERNAL);
  binding()->events().OnEstablished(std::move(request));
}

void IsoStreamServer::SetupDataPath(
    fuchsia::bluetooth::le::IsochronousStreamSetupDataPathRequest parameters,
    SetupDataPathCallback callback) {
  pw::bluetooth::emboss::DataPathDirection direction =
      fidl_helpers::DataPathDirectionFromFidl(parameters.data_direction());
  const char* direction_as_str =
      fidl_helpers::DataPathDirectionToString(direction);
  bt_log(INFO,
         "fidl",
         "Request received to set up data path (direction: %s)",
         direction_as_str);
  if (direction != pw::bluetooth::emboss::DataPathDirection::OUTPUT) {
    // We only support Controller => Host at the moment
    bt_log(WARN,
           "fidl",
           "Attempt to set up data path with unsupported direction: %s",
           direction_as_str);
    callback(fpromise::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter> codec_id =
      fidl_helpers::CodecIdFromFidl(parameters.codec_attributes().codec_id());
  std::optional<std::vector<uint8_t>> codec_configuration;
  if (parameters.codec_attributes().has_codec_configuration()) {
    codec_configuration = parameters.codec_attributes().codec_configuration();
  }

  zx::duration delay(parameters.controller_delay());
  uint32_t delay_in_us = delay.to_usecs();
  if (!iso_stream_.has_value()) {
    bt_log(WARN, "fidl", "data path setup failed (CIS not established)");
    callback(fpromise::error(ZX_ERR_BAD_STATE));
    return;
  }
  if (!iso_stream_->is_alive()) {
    bt_log(INFO, "fidl", "Attempt to set data path after CIS closed");
    callback(fpromise::error(ZX_ERR_BAD_STATE));
    return;
  }

  (*iso_stream_)
      ->SetupDataPath(
          direction,
          codec_id,
          codec_configuration,
          delay_in_us,
          [callback = std::move(callback)](
              bt::iso::IsoStream::SetupDataPathError error) {
            switch (error) {
              case bt::iso::IsoStream::kSuccess:
                bt_log(INFO, "fidl", "data path successfully setup");
                callback(fpromise::ok());
                break;
              case bt::iso::IsoStream::kStreamAlreadyExists:
                bt_log(WARN,
                       "fidl",
                       "data path setup failed (stream already setup)");
                callback(fpromise::error(ZX_ERR_ALREADY_EXISTS));
                break;
              case bt::iso::IsoStream::kCisNotEstablished:
                bt_log(WARN,
                       "fidl",
                       "data path setup failed (CIS not established)");
                callback(fpromise::error(ZX_ERR_BAD_STATE));
                break;
              case bt::iso::IsoStream::kInvalidArgs:
                bt_log(WARN,
                       "fidl",
                       "data path setup failed (invalid parameters)");
                callback(fpromise::error(ZX_ERR_INVALID_ARGS));
                break;
              case bt::iso::IsoStream::kStreamClosed:
                bt_log(WARN, "fidl", "data path setup failed (stream closed)");
                callback(fpromise::error(ZX_ERR_BAD_STATE));
                break;
              default:
                bt_log(ERROR,
                       "fidl",
                       "Unsupported case in SetupDataPathError: %u",
                       static_cast<unsigned>(error));
                callback(fpromise::error(ZX_ERR_INTERNAL));
                break;
            }
          });
}

void IsoStreamServer::Read(ReadCallback callback) {}

void IsoStreamServer::OnClosed() {
  if (iso_stream_.has_value() && iso_stream_->is_alive()) {
    (*iso_stream_)->Close();
  }
  // This may free our instance.
  on_closed_cb_();
}

void IsoStreamServer::Close(zx_status_t epitaph) {
  binding()->Close(epitaph);
  OnClosed();
}

void IsoStreamServer::handle_unknown_method(uint64_t ordinal,
                                            bool has_response) {
  bt_log(WARN,
         "fidl",
         "Received unknown fidl call %#" PRIx64 " (%s responses)",
         ordinal,
         has_response ? "with" : "without");
}

}  // namespace bthost