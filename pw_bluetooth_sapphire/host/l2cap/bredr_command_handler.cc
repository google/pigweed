// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_command_handler.h"

#include <endian.h>

#include <type_traits>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt {
namespace l2cap {
namespace internal {

bool BrEdrCommandHandler::ConnectionResponse::Decode(const ByteBuffer& payload_buf) {
  auto& conn_rsp_payload = payload_buf.As<PayloadT>();
  remote_cid_ = letoh16(conn_rsp_payload.dst_cid);
  local_cid_ = letoh16(conn_rsp_payload.src_cid);
  result_ = static_cast<ConnectionResult>(letoh16(conn_rsp_payload.result));
  conn_status_ = static_cast<ConnectionStatus>(letoh16(conn_rsp_payload.status));
  return true;
}

bool BrEdrCommandHandler::ConfigurationResponse::Decode(const ByteBuffer& payload_buf) {
  PacketView<PayloadT> config_rsp(&payload_buf, payload_buf.size() - sizeof(PayloadT));
  local_cid_ = letoh16(config_rsp.header().src_cid);
  flags_ = letoh16(config_rsp.header().flags);
  result_ = static_cast<ConfigurationResult>(letoh16(config_rsp.header().result));

  if (!config_.ReadOptions(config_rsp.payload_data())) {
    bt_log(WARN, "l2cap", "could not decode channel configuration response option");
    return false;
  }
  return true;
}

bool BrEdrCommandHandler::InformationResponse::Decode(const ByteBuffer& payload_buf) {
  PacketView<InformationResponsePayload> info_rsp(
      &payload_buf, payload_buf.size() - sizeof(InformationResponsePayload));
  type_ = InformationType{letoh16(info_rsp.header().type)};
  result_ = InformationResult{letoh16(info_rsp.header().result)};
  if (result_ != InformationResult::kSuccess) {
    return true;
  }

  size_t expected_size = 0;
  switch (type_) {
    case InformationType::kConnectionlessMTU:
      expected_size = sizeof(uint16_t);
      break;
    case InformationType::kExtendedFeaturesSupported:
      expected_size = sizeof(ExtendedFeatures);
      break;
    case InformationType::kFixedChannelsSupported:
      expected_size = sizeof(FixedChannelsSupported);
      break;
    default:
      bt_log(DEBUG, "l2cap-bredr",
             "cmd: passing Information Response with unknown type %#.4hx with %zu data bytes",
             type_, info_rsp.payload_size());
  }
  if (info_rsp.payload_size() < expected_size) {
    bt_log(DEBUG, "l2cap-bredr",
           "cmd: ignoring malformed Information Response, type %#.4hx with %zu data bytes", type_,
           info_rsp.payload_size());
    return false;
  }
  data_ = info_rsp.payload_data();
  return true;
}

BrEdrCommandHandler::ConnectionResponder::ConnectionResponder(
    SignalingChannel::Responder* sig_responder, ChannelId remote_cid)
    : Responder(sig_responder, kInvalidChannelId, remote_cid) {}

void BrEdrCommandHandler::ConnectionResponder::Send(ChannelId local_cid, ConnectionResult result,
                                                    ConnectionStatus status) {
  ConnectionResponsePayload conn_rsp = {htole16(local_cid), htole16(remote_cid()),
                                        static_cast<ConnectionResult>(htole16(result)),
                                        static_cast<ConnectionStatus>(htole16(status))};
  sig_responder_->Send(BufferView(&conn_rsp, sizeof(conn_rsp)));
}

BrEdrCommandHandler::ConfigurationResponder::ConfigurationResponder(
    SignalingChannel::Responder* sig_responder, ChannelId local_cid)
    : Responder(sig_responder, local_cid) {}

void BrEdrCommandHandler::ConfigurationResponder::Send(
    ChannelId remote_cid, uint16_t flags, ConfigurationResult result,
    ChannelConfiguration::ConfigurationOptions options) {
  size_t options_size = 0;
  for (auto& option : options) {
    options_size += option->size();
  }

  DynamicByteBuffer config_rsp_buf(sizeof(ConfigurationResponsePayload) + options_size);
  MutablePacketView<ConfigurationResponsePayload> config_rsp(&config_rsp_buf, options_size);
  config_rsp.mutable_header()->src_cid = htole16(remote_cid);
  config_rsp.mutable_header()->flags = htole16(flags);
  config_rsp.mutable_header()->result = static_cast<ConfigurationResult>(htole16(result));

  auto payload_view = config_rsp.mutable_payload_data().mutable_view();
  for (auto& option : options) {
    auto encoded = option->Encode();
    payload_view.Write(encoded.data(), encoded.size());
    payload_view = payload_view.mutable_view(encoded.size());
  }

  sig_responder_->Send(config_rsp.data());
}

BrEdrCommandHandler::DisconnectionResponder::DisconnectionResponder(
    SignalingChannel::Responder* sig_responder, ChannelId local_cid, ChannelId remote_cid)
    : Responder(sig_responder, local_cid, remote_cid) {}

void BrEdrCommandHandler::DisconnectionResponder::Send() {
  DisconnectionResponsePayload discon_rsp = {htole16(local_cid()), htole16(remote_cid())};
  sig_responder_->Send(BufferView(&discon_rsp, sizeof(discon_rsp)));
}

BrEdrCommandHandler::InformationResponder::InformationResponder(
    SignalingChannel::Responder* sig_responder, InformationType type)
    : Responder(sig_responder), type_(type) {}

void BrEdrCommandHandler::InformationResponder::SendNotSupported() {
  Send(InformationResult::kNotSupported, BufferView());
}

void BrEdrCommandHandler::InformationResponder::SendConnectionlessMtu(uint16_t mtu) {
  mtu = htole16(mtu);
  Send(InformationResult::kSuccess, BufferView(&mtu, sizeof(mtu)));
}

void BrEdrCommandHandler::InformationResponder::SendExtendedFeaturesSupported(
    ExtendedFeatures extended_features) {
  extended_features = htole32(extended_features);
  Send(InformationResult::kSuccess, BufferView(&extended_features, sizeof(extended_features)));
}

void BrEdrCommandHandler::InformationResponder::SendFixedChannelsSupported(
    FixedChannelsSupported channels_supported) {
  channels_supported = htole64(channels_supported);
  Send(InformationResult::kSuccess, BufferView(&channels_supported, sizeof(channels_supported)));
}

void BrEdrCommandHandler::InformationResponder::Send(InformationResult result,
                                                     const ByteBuffer& data) {
  constexpr size_t kMaxPayloadLength = sizeof(InformationResponsePayload) + sizeof(uint64_t);
  StaticByteBuffer<kMaxPayloadLength> info_rsp_buf;
  MutablePacketView<InformationResponsePayload> info_rsp_view(&info_rsp_buf, data.size());

  info_rsp_view.mutable_header()->type = static_cast<InformationType>(htole16(type_));
  info_rsp_view.mutable_header()->result = static_cast<InformationResult>(htole16(result));
  info_rsp_view.mutable_payload_data().Write(data);
  sig_responder_->Send(info_rsp_view.data());
}

BrEdrCommandHandler::BrEdrCommandHandler(SignalingChannelInterface* sig,
                                         fit::closure request_fail_callback)
    : CommandHandler(sig, std::move(request_fail_callback)) {}

bool BrEdrCommandHandler::SendConnectionRequest(uint16_t psm, ChannelId local_cid,
                                                ConnectionResponseCallback cb) {
  auto on_conn_rsp = BuildResponseHandler<ConnectionResponse>(std::move(cb));

  ConnectionRequestPayload payload = {htole16(psm), htole16(local_cid)};
  return sig()->SendRequest(kConnectionRequest, BufferView(&payload, sizeof(payload)),
                            std::move(on_conn_rsp));
}

bool BrEdrCommandHandler::SendConfigurationRequest(
    ChannelId remote_cid, uint16_t flags, ChannelConfiguration::ConfigurationOptions options,
    ConfigurationResponseCallback cb) {
  auto on_config_rsp = BuildResponseHandler<ConfigurationResponse>(std::move(cb));

  size_t options_size = 0;
  for (auto& option : options) {
    options_size += option->size();
  }

  DynamicByteBuffer config_req_buf(sizeof(ConfigurationRequestPayload) + options_size);
  MutablePacketView<ConfigurationRequestPayload> config_req(&config_req_buf, options_size);
  config_req.mutable_header()->dst_cid = htole16(remote_cid);
  config_req.mutable_header()->flags = htole16(flags);

  auto payload_view = config_req.mutable_payload_data().mutable_view();
  for (auto& option : options) {
    auto encoded = option->Encode();
    payload_view.Write(encoded.data(), encoded.size());
    payload_view = payload_view.mutable_view(encoded.size());
  }

  return sig()->SendRequest(kConfigurationRequest, config_req_buf, std::move(on_config_rsp));
}

bool BrEdrCommandHandler::SendInformationRequest(InformationType type,
                                                 InformationResponseCallback cb) {
  auto on_info_rsp = BuildResponseHandler<InformationResponse>(std::move(cb));

  InformationRequestPayload payload = {InformationType{htole16(type)}};
  return sig()->SendRequest(kInformationRequest, BufferView(&payload, sizeof(payload)),
                            std::move(on_info_rsp));
}

void BrEdrCommandHandler::ServeConnectionRequest(ConnectionRequestCallback cb) {
  auto on_conn_req = [cb = std::move(cb)](const ByteBuffer& request_payload,
                                          SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() != sizeof(ConnectionRequestPayload)) {
      bt_log(DEBUG, "l2cap-bredr", "cmd: rejecting malformed Connection Request, size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    const auto& conn_req = request_payload.As<ConnectionRequestPayload>();
    const PSM psm = letoh16(conn_req.psm);
    const ChannelId remote_cid = letoh16(conn_req.src_cid);

    ConnectionResponder responder(sig_responder, remote_cid);

    // v5.0 Vol 3, Part A, Sec 4.2: PSMs shall be odd and the least significant
    // bit of the most significant byte shall be zero
    if (((psm & 0x0001) != 0x0001) || ((psm & 0x0100) != 0x0000)) {
      bt_log(DEBUG, "l2cap-bredr", "Rejecting connection for invalid PSM %#.4x from channel %#.4x",
             psm, remote_cid);
      responder.Send(kInvalidChannelId, ConnectionResult::kPSMNotSupported,
                     ConnectionStatus::kNoInfoAvailable);
      return;
    }

    // Check that source channel ID is in range (v5.0 Vol 3, Part A, Sec 2.1)
    if (remote_cid < kFirstDynamicChannelId) {
      bt_log(DEBUG, "l2cap-bredr", "Rejecting connection for PSM %#.4x from invalid channel %#.4x",
             psm, remote_cid);
      responder.Send(kInvalidChannelId, ConnectionResult::kInvalidSourceCID,
                     ConnectionStatus::kNoInfoAvailable);
      return;
    }

    cb(psm, remote_cid, &responder);
  };

  sig()->ServeRequest(kConnectionRequest, std::move(on_conn_req));
}

void BrEdrCommandHandler::ServeConfigurationRequest(ConfigurationRequestCallback cb) {
  auto on_config_req = [cb = std::move(cb)](const ByteBuffer& request_payload,
                                            SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() < sizeof(ConfigurationRequestPayload)) {
      bt_log(DEBUG, "l2cap-bredr", "cmd: rejecting malformed Configuration Request, size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    PacketView<ConfigurationRequestPayload> config_req(
        &request_payload, request_payload.size() - sizeof(ConfigurationRequestPayload));
    const auto local_cid = static_cast<ChannelId>(letoh16(config_req.header().dst_cid));
    const uint16_t flags = letoh16(config_req.header().flags);
    ConfigurationResponder responder(sig_responder, local_cid);

    ChannelConfiguration config;
    if (!config.ReadOptions(config_req.payload_data())) {
      bt_log(WARN, "l2cap", "could not decode configuration option in configuration request");
    }

    cb(local_cid, flags, std::move(config), &responder);
  };

  sig()->ServeRequest(kConfigurationRequest, std::move(on_config_req));
}

void BrEdrCommandHandler::ServeInformationRequest(InformationRequestCallback cb) {
  auto on_info_req = [cb = std::move(cb)](const ByteBuffer& request_payload,
                                          SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() != sizeof(InformationRequestPayload)) {
      bt_log(DEBUG, "l2cap-bredr", "cmd: rejecting malformed Information Request, size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    const auto& info_req = request_payload.As<InformationRequestPayload>();
    const auto type = static_cast<InformationType>(letoh16(info_req.type));
    InformationResponder responder(sig_responder, type);
    cb(type, &responder);
  };

  sig()->ServeRequest(kInformationRequest, std::move(on_info_req));
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
