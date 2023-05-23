// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include <string.h>
#include <zircon/status.h>
#include <zircon/syscalls/clock.h>

#include <cstddef>
#include <memory>

#include "fuchsia/bluetooth/bredr/cpp/fidl.h"
#include "helpers.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_ptr.h"
#include "lib/fpromise/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/host_error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/common/weak_self.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "zircon/errors.h"

namespace fidlbredr = fuchsia::bluetooth::bredr;
namespace hci_android = bt::hci_spec::vendor::android;
using fidlbredr::DataElement;
using fidlbredr::Profile;
using pw::bluetooth::AclPriority;
using FeaturesBits = pw::bluetooth::Controller::FeaturesBits;

namespace bthost {

namespace {

bt::l2cap::ChannelParameters FidlToChannelParameters(const fidlbredr::ChannelParameters& fidl) {
  bt::l2cap::ChannelParameters params;
  if (fidl.has_channel_mode()) {
    switch (fidl.channel_mode()) {
      case fidlbredr::ChannelMode::BASIC:
        params.mode = bt::l2cap::ChannelMode::kBasic;
        break;
      case fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION:
        params.mode = bt::l2cap::ChannelMode::kEnhancedRetransmission;
        break;
      default:
        BT_PANIC("FIDL channel parameter contains invalid mode");
    }
  }
  if (fidl.has_max_rx_sdu_size()) {
    params.max_rx_sdu_size = fidl.max_rx_sdu_size();
  }
  if (fidl.has_flush_timeout()) {
    params.flush_timeout = zx::duration(fidl.flush_timeout());
  }
  return params;
}

fidlbredr::ChannelMode ChannelModeToFidl(bt::l2cap::ChannelMode mode) {
  switch (mode) {
    case bt::l2cap::ChannelMode::kBasic:
      return fidlbredr::ChannelMode::BASIC;
      break;
    case bt::l2cap::ChannelMode::kEnhancedRetransmission:
      return fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION;
      break;
    default:
      BT_PANIC("Could not convert channel parameter mode to unsupported FIDL mode");
  }
}

fidlbredr::ChannelParameters ChannelInfoToFidlChannelParameters(
    const bt::l2cap::ChannelInfo& info) {
  fidlbredr::ChannelParameters params;
  params.set_channel_mode(ChannelModeToFidl(info.mode));
  params.set_max_rx_sdu_size(info.max_rx_sdu_size);
  if (info.flush_timeout) {
    params.set_flush_timeout(info.flush_timeout.value().get());
  }
  return params;
}

// NOLINTNEXTLINE(misc-no-recursion)
fidlbredr::DataElementPtr DataElementToFidl(const bt::sdp::DataElement* in) {
  auto elem = std::make_unique<fidlbredr::DataElement>();
  bt_log(TRACE, "fidl", "DataElementToFidl: %s", in->ToString().c_str());
  BT_DEBUG_ASSERT(in);
  switch (in->type()) {
    case bt::sdp::DataElement::Type::kUnsignedInt: {
      switch (in->size()) {
        case bt::sdp::DataElement::Size::kOneByte:
          elem->set_uint8(*in->Get<uint8_t>());
          break;
        case bt::sdp::DataElement::Size::kTwoBytes:
          elem->set_uint16(*in->Get<uint16_t>());
          break;
        case bt::sdp::DataElement::Size::kFourBytes:
          elem->set_uint32(*in->Get<uint32_t>());
          break;
        case bt::sdp::DataElement::Size::kEightBytes:
          elem->set_uint64(*in->Get<uint64_t>());
          break;
        default:
          bt_log(INFO, "fidl", "no 128-bit integer support in FIDL yet");
          return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kSignedInt: {
      switch (in->size()) {
        case bt::sdp::DataElement::Size::kOneByte:
          elem->set_int8(*in->Get<int8_t>());
          break;
        case bt::sdp::DataElement::Size::kTwoBytes:
          elem->set_int16(*in->Get<int16_t>());
          break;
        case bt::sdp::DataElement::Size::kFourBytes:
          elem->set_int32(*in->Get<int32_t>());
          break;
        case bt::sdp::DataElement::Size::kEightBytes:
          elem->set_int64(*in->Get<int64_t>());
          break;
        default:
          bt_log(INFO, "fidl", "no 128-bit integer support in FIDL yet");
          return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kUuid: {
      auto uuid = in->Get<bt::UUID>();
      BT_DEBUG_ASSERT(uuid);
      elem->set_uuid(fidl_helpers::UuidToFidl(*uuid));
      return elem;
    }
    case bt::sdp::DataElement::Type::kString: {
      auto bytes = in->Get<bt::DynamicByteBuffer>();
      BT_DEBUG_ASSERT(bytes);
      std::vector<uint8_t> data(bytes->cbegin(), bytes->cend());
      elem->set_str(data);
      return elem;
    }
    case bt::sdp::DataElement::Type::kBoolean: {
      elem->set_b(*in->Get<bool>());
      return elem;
    }
    case bt::sdp::DataElement::Type::kSequence: {
      std::vector<fidlbredr::DataElementPtr> elems;
      const bt::sdp::DataElement* it;
      for (size_t idx = 0; (it = in->At(idx)); ++idx) {
        elems.emplace_back(DataElementToFidl(it));
      }
      elem->set_sequence(std::move(elems));
      return elem;
    }
    case bt::sdp::DataElement::Type::kAlternative: {
      std::vector<fidlbredr::DataElementPtr> elems;
      const bt::sdp::DataElement* it;
      for (size_t idx = 0; (it = in->At(idx)); ++idx) {
        elems.emplace_back(DataElementToFidl(it));
      }
      elem->set_alternatives(std::move(elems));
      return elem;
    }
    case bt::sdp::DataElement::Type::kUrl: {
      elem->set_url(*in->GetUrl());
      return elem;
    }
    case bt::sdp::DataElement::Type::kNull: {
      bt_log(INFO, "fidl", "no support for null DataElement types in FIDL");
      return nullptr;
    }
  }
}

fidlbredr::ProtocolDescriptorPtr DataElementToProtocolDescriptor(const bt::sdp::DataElement* in) {
  auto desc = std::make_unique<fidlbredr::ProtocolDescriptor>();
  if (in->type() != bt::sdp::DataElement::Type::kSequence) {
    bt_log(DEBUG, "fidl", "DataElement type is not kSequence (in: %s)", bt_str(*in));
    return nullptr;
  }
  const auto protocol_uuid = in->At(0)->Get<bt::UUID>();
  if (!protocol_uuid) {
    bt_log(DEBUG, "fidl", "first DataElement in sequence is not type kUUID (in: %s)", bt_str(*in));
    return nullptr;
  }
  desc->protocol = static_cast<fidlbredr::ProtocolIdentifier>(*protocol_uuid->As16Bit());
  const bt::sdp::DataElement* it;
  for (size_t idx = 1; (it = in->At(idx)); ++idx) {
    desc->params.push_back(std::move(*DataElementToFidl(it)));
  }

  return desc;
}

AclPriority FidlToAclPriority(fidlbredr::A2dpDirectionPriority in) {
  switch (in) {
    case fidlbredr::A2dpDirectionPriority::SOURCE:
      return AclPriority::kSource;
    case fidlbredr::A2dpDirectionPriority::SINK:
      return AclPriority::kSink;
    default:
      return AclPriority::kNormal;
  }
}

}  // namespace

ProfileServer::ProfileServer(bt::gap::Adapter::WeakPtr adapter,
                             fidl::InterfaceRequest<Profile> request)
    : ServerBase(this, std::move(request)),
      advertised_total_(0),
      searches_total_(0),
      adapter_(std::move(adapter)),
      weak_self_(this) {}

ProfileServer::~ProfileServer() {
  if (adapter().is_alive()) {
    // Unregister anything that we have registered.
    for (const auto& it : current_advertised_) {
      adapter()->bredr()->UnregisterService(it.second.registration_handle);
      it.second.disconnection_cb(fpromise::ok());
    }
    for (const auto& it : searches_) {
      adapter()->bredr()->RemoveServiceSearch(it.second.search_id);
    }
  }
}

void ProfileServer::L2capParametersExt::RequestParameters(
    fuchsia::bluetooth::bredr::ChannelParameters requested, RequestParametersCallback callback) {
  if (requested.has_flush_timeout()) {
    channel_->SetBrEdrAutomaticFlushTimeout(
        zx::duration(requested.flush_timeout()),
        [chan = channel_, cb = std::move(callback)](auto result) {
          if (result.is_ok()) {
            bt_log(DEBUG, "fidl",
                   "L2capParametersExt::RequestParameters: setting flush timeout succeeded");
          } else {
            bt_log(INFO, "fidl",
                   "L2capParametersExt::RequestParameters: setting flush timeout failed");
          }
          // Return the current parameters even if the request failed.
          // TODO(fxb/73039): set current security requirements in returned channel parameters
          cb(ChannelInfoToFidlChannelParameters(chan->info()));
        });
    return;
  }

  // No other channel parameters are  supported, so just return the current parameters.
  // TODO(fxb/73039): set current security requirements in returned channel parameters
  callback(ChannelInfoToFidlChannelParameters(channel_->info()));
}

void ProfileServer::AudioOffloadExt::GetSupportedFeatures(GetSupportedFeaturesCallback callback) {
  fidlbredr::AudioOffloadExtGetSupportedFeaturesResponse response;
  std::vector<fidlbredr::AudioOffloadFeatures>* mutable_audio_offload_features =
      response.mutable_audio_offload_features();
  const bt::gap::AdapterState& adapter_state = adapter_->state();

  if (!adapter_state.IsControllerFeatureSupported(FeaturesBits::kAndroidVendorExtensions)) {
    callback(std::move(response));
    return;
  }

  const uint32_t a2dp_offload_capabilities =
      adapter_state.android_vendor_capabilities.a2dp_source_offload_capability_mask();
  const uint32_t sbc_capability = static_cast<uint32_t>(hci_android::A2dpCodecType::kSbc);
  const uint32_t aac_capability = static_cast<uint32_t>(hci_android::A2dpCodecType::kAac);

  if (a2dp_offload_capabilities & sbc_capability) {
    fidlbredr::AudioSbcSupport audio_sbc_support;
    mutable_audio_offload_features->push_back(
        fidlbredr::AudioOffloadFeatures::WithSbc(std::move(audio_sbc_support)));
  }
  if (a2dp_offload_capabilities & aac_capability) {
    fidlbredr::AudioAacSupport audio_aac_support;
    mutable_audio_offload_features->push_back(
        fidlbredr::AudioOffloadFeatures::WithAac(std::move(audio_aac_support)));
  }

  callback(std::move(response));
}

void ProfileServer::AudioOffloadExt::StartAudioOffload(
    fidlbredr::AudioOffloadConfiguration audio_offload_configuration,
    fidl::InterfaceRequest<fidlbredr::AudioOffloadController> controller) {
  auto audio_offload_controller_server =
      std::make_unique<AudioOffloadController>(std::move(controller), channel_);
  WeakPtr<AudioOffloadController> server_ptr = audio_offload_controller_server->GetWeakPtr();

  std::unique_ptr<bt::l2cap::A2dpOffloadManager::Configuration> config =
      AudioOffloadConfigFromFidl(audio_offload_configuration);
  if (!config) {
    bt_log(ERROR, "fidl", "%s: invalid config received", __FUNCTION__);
    server_ptr->Close(/*epitaph_value=*/ZX_ERR_NOT_SUPPORTED);
    return;
  }

  auto error_handler = [this, server_ptr](zx_status_t status) {
    if (!server_ptr.is_alive()) {
      bt_log(ERROR, "fidl", "audio offload controller server was destroyed");
      return;
    }

    bt_log(DEBUG, "fidl", "audio offload controller server closed (reason: %s)",
           zx_status_get_string(status));
    if (!profile_server_.audio_offload_controller_server_) {
      bt_log(WARN, "fidl",
             "could not find controller server in audio offload controller error callback");
    }

    bt::hci::ResultCallback<> stop_cb =
        [server_ptr](fit::result<bt::Error<pw::bluetooth::emboss::StatusCode>> result) {
          if (result.is_error()) {
            bt_log(ERROR, "fidl", "stopping audio offload failed in error handler: %s",
                   bt_str(result));
            server_ptr->Close(/*epitaph_value=*/ZX_ERR_UNAVAILABLE);
            return;
          }
          bt_log(ERROR, "fidl", "stopping audio offload complete: %s", bt_str(result));
        };
    channel_->StopA2dpOffload(std::move(stop_cb));
  };
  audio_offload_controller_server->set_error_handler(error_handler);
  profile_server_.audio_offload_controller_server_ = std::move(audio_offload_controller_server);

  auto callback = [this,
                   server_ptr](fit::result<bt::Error<pw::bluetooth::emboss::StatusCode>> result) {
    if (!server_ptr.is_alive()) {
      bt_log(ERROR, "fidl", "audio offload controller server was destroyed");
      return;
    }
    if (result.is_error()) {
      bt_log(ERROR, "fidl", "StartAudioOffload failed: %s", bt_str(result));

      auto host_error = result.error_value().host_error();
      if (host_error == bt::HostError::kInProgress) {
        server_ptr->Close(/*epitaph_value=*/ZX_ERR_ALREADY_BOUND);
      } else if (host_error == bt::HostError::kFailed) {
        server_ptr->Close(/*epitaph_value=*/ZX_ERR_INTERNAL);
      } else {
        server_ptr->Close(/*epitaph_value=*/ZX_ERR_UNAVAILABLE);
      }
      profile_server_.audio_offload_controller_server_ = nullptr;
      return;
    }
    // Send OnStarted event to tell Rust Profiles that we've finished offloading
    server_ptr->SendOnStartedEvent();
  };
  channel_->StartA2dpOffload(*config, std::move(callback));
}

std::unique_ptr<bt::l2cap::A2dpOffloadManager::Configuration>
ProfileServer::AudioOffloadExt::AudioOffloadConfigFromFidl(
    fidlbredr::AudioOffloadConfiguration& audio_offload_configuration) {
  auto codec = fidl_helpers::FidlToCodecType(audio_offload_configuration.codec());
  if (!codec.has_value()) {
    bt_log(WARN, "fidl", "%s: invalid codec", __FUNCTION__);
    return nullptr;
  }

  std::unique_ptr<bt::l2cap::A2dpOffloadManager::Configuration> config =
      std::make_unique<bt::l2cap::A2dpOffloadManager::Configuration>();

  config->codec = codec.value();
  config->max_latency = audio_offload_configuration.max_latency();
  config->scms_t_enable =
      fidl_helpers::FidlToScmsTEnable(audio_offload_configuration.scms_t_enable());
  config->sampling_frequency =
      fidl_helpers::FidlToSamplingFrequency(audio_offload_configuration.sampling_frequency());
  config->bits_per_sample =
      fidl_helpers::FidlToBitsPerSample(audio_offload_configuration.bits_per_sample());
  config->channel_mode =
      fidl_helpers::FidlToChannelMode(audio_offload_configuration.channel_mode());
  config->encoded_audio_bit_rate = audio_offload_configuration.encoded_bit_rate();
  config->codec_information = fidl_helpers::FidlToEncoderSettings(
      audio_offload_configuration.encoder_settings(),
      audio_offload_configuration.sampling_frequency(), audio_offload_configuration.channel_mode());

  return config;
}

ProfileServer::ScoConnectionServer::ScoConnectionServer(
    fidl::InterfaceRequest<fuchsia::bluetooth::bredr::ScoConnection> request,
    bt::sco::ScoConnection::WeakPtr connection)
    : ServerBase(this, std::move(request)), connection_(std::move(connection)) {
  binding()->set_error_handler([this](zx_status_t) { Close(ZX_ERR_CANCELED); });
}

ProfileServer::ScoConnectionServer::~ScoConnectionServer() {
  if (connection_.is_alive()) {
    connection_->Deactivate();
  }
}

void ProfileServer::ScoConnectionServer::Activate(fit::callback<void()> on_closed) {
  on_closed_ = std::move(on_closed);

  auto rx_callback = [this] { TryRead(); };
  auto closed_cb = [this] { Close(ZX_ERR_PEER_CLOSED); };
  connection_->Activate(std::move(rx_callback), std::move(closed_cb));
}

void ProfileServer::ScoConnectionServer::Read(ReadCallback callback) {
  if (connection_->parameters().view().input_data_path().Read() !=
      pw::bluetooth::emboss::ScoDataPath::HCI) {
    bt_log(WARN, "fidl", "%s called for an offloaded SCO connection", __func__);
    Close(ZX_ERR_IO_NOT_PRESENT);
    return;
  }

  if (read_cb_) {
    bt_log(WARN, "fidl", "%s called when a read callback was already present", __func__);
    Close(ZX_ERR_BAD_STATE);
    return;
  }
  read_cb_ = std::move(callback);
  TryRead();
}

void ProfileServer::ScoConnectionServer::Write(std::vector<uint8_t> data, WriteCallback callback) {
  if (connection_->parameters().view().output_data_path().Read() !=
      pw::bluetooth::emboss::ScoDataPath::HCI) {
    bt_log(WARN, "fidl", "%s called for a non-HCI SCO connection", __func__);
    Close(ZX_ERR_IO_NOT_PRESENT);
    return;
  }

  auto buffer = std::make_unique<bt::DynamicByteBuffer>(data.size());
  buffer->Write(data.data(), data.size());
  if (!connection_->Send(std::move(buffer))) {
    bt_log(WARN, "fidl", "%s: failed to send SCO packet", __func__);
    Close(ZX_ERR_IO);
    return;
  }
  callback();
}

void ProfileServer::ScoConnectionServer::TryRead() {
  if (!read_cb_) {
    return;
  }
  std::unique_ptr<bt::hci::ScoDataPacket> packet = connection_->Read();
  if (!packet) {
    return;
  }
  std::vector<uint8_t> payload;
  fuchsia::bluetooth::bredr::RxPacketStatus status =
      fidl_helpers::ScoPacketStatusToFidl(packet->packet_status_flag());
  if (packet->packet_status_flag() !=
      bt::hci_spec::SynchronousDataPacketStatusFlag::kNoDataReceived) {
    payload = packet->view().payload_data().ToVector();
  }
  read_cb_(status, std::move(payload));
}

void ProfileServer::ScoConnectionServer::Close(zx_status_t epitaph) {
  connection_->Deactivate();
  connection_.reset();
  binding()->Close(epitaph);
  if (on_closed_) {
    on_closed_();
  }
}

void ProfileServer::Advertise(
    std::vector<fidlbredr::ServiceDefinition> definitions, fidlbredr::ChannelParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver,
    AdvertiseCallback callback) {
  std::vector<bt::sdp::ServiceRecord> registering;

  for (auto& definition : definitions) {
    auto rec = fidl_helpers::ServiceDefinitionToServiceRecord(definition);
    // Drop the receiver on error.
    if (rec.is_error()) {
      bt_log(WARN, "fidl", "%s: Failed to create service record from service defintion",
             __FUNCTION__);
      callback(fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS));
      return;
    }
    registering.emplace_back(std::move(rec.value()));
  }

  BT_ASSERT(adapter().is_alive());
  BT_ASSERT(adapter()->bredr());

  uint64_t next = advertised_total_ + 1;

  auto registration_handle = adapter()->bredr()->RegisterService(
      std::move(registering), FidlToChannelParameters(parameters),
      [this, next](auto channel, const auto& protocol_list) {
        OnChannelConnected(next, std::move(channel), std::move(protocol_list));
      });

  if (!registration_handle) {
    bt_log(WARN, "fidl", "%s: Failed to register service", __FUNCTION__);
    callback(fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS));
    return;
  };

  auto receiverptr = receiver.Bind();

  receiverptr.set_error_handler(
      [this, next](zx_status_t /*status*/) { OnConnectionReceiverError(next); });

  current_advertised_.try_emplace(next, std::move(receiverptr), registration_handle,
                                  std::move(callback));
  advertised_total_ = next;
}

void ProfileServer::Search(
    fidlbredr::ServiceClassProfileIdentifier service_uuid, std::vector<uint16_t> attr_ids,
    fidl::InterfaceHandle<fuchsia::bluetooth::bredr::SearchResults> results) {
  bt::UUID search_uuid(static_cast<uint32_t>(service_uuid));
  std::unordered_set<bt::sdp::AttributeId> attributes(attr_ids.begin(), attr_ids.end());
  if (!attr_ids.empty()) {
    // Always request the ProfileDescriptor for the event
    attributes.insert(bt::sdp::kBluetoothProfileDescriptorList);
  }

  BT_DEBUG_ASSERT(adapter().is_alive());

  auto next = searches_total_ + 1;

  auto search_id = adapter()->bredr()->AddServiceSearch(
      search_uuid, std::move(attributes),
      [this, next](auto id, const auto& attrs) { OnServiceFound(next, id, attrs); });

  if (!search_id) {
    return;
  }

  auto results_ptr = results.Bind();
  results_ptr.set_error_handler(
      [this, next](zx_status_t status) { OnSearchResultError(next, status); });

  searches_.try_emplace(next, std::move(results_ptr), search_id);
  searches_total_ = next;
}

void ProfileServer::Connect(fuchsia::bluetooth::PeerId peer_id,
                            fidlbredr::ConnectParameters connection, ConnectCallback callback) {
  bt::PeerId id{peer_id.value};

  // Anything other than L2CAP is not supported by this server.
  if (!connection.is_l2cap()) {
    bt_log(WARN, "fidl", "%s: non-l2cap connections are not supported (is_rfcomm: %d, peer: %s)",
           __FUNCTION__, connection.is_rfcomm(), bt_str(id));
    callback(fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS));
    return;
  }

  // The L2CAP parameters must include a PSM. ChannelParameters are optional.
  auto l2cap_params = std::move(connection.l2cap());
  if (!l2cap_params.has_psm()) {
    bt_log(WARN, "fidl", "%s: missing l2cap psm (peer: %s)", __FUNCTION__, bt_str(id));
    callback(fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS));
    return;
  }
  uint16_t psm = l2cap_params.psm();

  fidlbredr::ChannelParameters parameters = std::move(*l2cap_params.mutable_parameters());

  auto connected_cb = [self = weak_self_.GetWeakPtr(), cb = callback.share(),
                       id](bt::l2cap::Channel::WeakPtr chan) {
    if (!chan.is_alive()) {
      bt_log(INFO, "fidl", "Connect: Channel socket is empty, returning failed. (peer: %s)",
             bt_str(id));
      cb(fpromise::error(fuchsia::bluetooth::ErrorCode::FAILED));
      return;
    }

    if (!self.is_alive()) {
      cb(fpromise::error(fuchsia::bluetooth::ErrorCode::FAILED));
      return;
    }

    auto fidl_chan = self->ChannelToFidl(std::move(chan));

    cb(fpromise::ok(std::move(fidl_chan)));
  };
  BT_DEBUG_ASSERT(adapter().is_alive());

  adapter()->bredr()->OpenL2capChannel(
      id, psm, fidl_helpers::FidlToBrEdrSecurityRequirements(parameters),
      FidlToChannelParameters(parameters), std::move(connected_cb));
}

void ProfileServer::ConnectSco(fuchsia::bluetooth::PeerId fidl_peer_id, bool initiator,
                               std::vector<fidlbredr::ScoConnectionParameters> fidl_params,
                               fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver) {
  bt::PeerId peer_id(fidl_peer_id.value);
  auto client = receiver.Bind();

  if (fidl_params.empty()) {
    bt_log(WARN, "fidl", "%s: empty parameters (peer: %s)", __FUNCTION__, bt_str(peer_id));
    client->Error(fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
    return;
  }

  if (initiator && fidl_params.size() != 1u) {
    bt_log(WARN, "fidl", "%s: too many parameters in initiator request (peer: %s)", __FUNCTION__,
           bt_str(peer_id));
    client->Error(fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
    return;
  }

  auto params_result = fidl_helpers::FidlToScoParametersVector(fidl_params);
  if (params_result.is_error()) {
    bt_log(WARN, "fidl", "%s: invalid parameters (peer: %s)", __FUNCTION__, bt_str(peer_id));
    client->Error(fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
    return;
  }
  auto params = params_result.value();

  auto request = fbl::MakeRefCounted<ScoRequest>();
  client.set_error_handler([request](zx_status_t status) { request->request_handle.reset(); });
  request->receiver = std::move(client);
  request->parameters = std::move(fidl_params);

  if (initiator) {
    auto callback = [self = weak_self_.GetWeakPtr(),
                     request](bt::sco::ScoConnectionManager::OpenConnectionResult result) mutable {
      // The connection may complete after this server is destroyed.
      if (!self.is_alive()) {
        // Prevent leaking connections.
        if (result.is_ok()) {
          result.value()->Deactivate();
        }
        return;
      }

      // Convert result type.
      if (result.is_error()) {
        self->OnScoConnectionResult(std::move(request), result.take_error());
        return;
      }
      self->OnScoConnectionResult(
          std::move(request),
          fit::ok(std::make_pair(std::move(result.value()), /*parameter index=*/0u)));
    };

    request->request_handle =
        adapter()->bredr()->OpenScoConnection(peer_id, params.front(), std::move(callback));
    return;
  }
  auto callback = [self = weak_self_.GetWeakPtr(),
                   request](bt::sco::ScoConnectionManager::AcceptConnectionResult result) mutable {
    // The connection may complete after this server is destroyed.
    if (!self.is_alive()) {
      // Prevent leaking connections.
      if (result.is_ok()) {
        result.value().first->Deactivate();
      }
      return;
    }

    self->OnScoConnectionResult(std::move(request), std::move(result));
  };
  request->request_handle =
      adapter()->bredr()->AcceptScoConnection(peer_id, params, std::move(callback));
}

void ProfileServer::OnChannelConnected(uint64_t ad_id, bt::l2cap::Channel::WeakPtr channel,
                                       const bt::sdp::DataElement& protocol_list) {
  auto it = current_advertised_.find(ad_id);
  if (it == current_advertised_.end()) {
    // The receiver has disappeared, do nothing.
    return;
  }

  BT_DEBUG_ASSERT(adapter().is_alive());
  auto handle = channel->link_handle();
  auto id = adapter()->bredr()->GetPeerId(handle);

  // The protocol that is connected should be L2CAP, because that is the only thing that
  // we can connect. We can't say anything about what the higher level protocols will be.
  auto prot_seq = protocol_list.At(0);
  BT_ASSERT(prot_seq);

  fidlbredr::ProtocolDescriptorPtr desc = DataElementToProtocolDescriptor(prot_seq);
  BT_ASSERT(desc);

  fuchsia::bluetooth::PeerId peer_id{id.value()};

  std::vector<fidlbredr::ProtocolDescriptor> list;
  list.emplace_back(std::move(*desc));

  auto fidl_chan = ChannelToFidl(std::move(channel));

  it->second.receiver->Connected(peer_id, std::move(fidl_chan), std::move(list));
}

void ProfileServer::OnConnectionReceiverError(uint64_t ad_id) {
  bt_log(DEBUG, "fidl", "Connection receiver closed, ending advertisement %lu", ad_id);

  auto it = current_advertised_.find(ad_id);

  if (it == current_advertised_.end() || !adapter().is_alive()) {
    return;
  }

  adapter()->bredr()->UnregisterService(it->second.registration_handle);
  it->second.disconnection_cb(fpromise::ok());

  current_advertised_.erase(it);
}

void ProfileServer::OnSearchResultError(uint64_t search_id, zx_status_t status) {
  bt_log(DEBUG, "fidl", "Search result closed, ending search %lu reason %s", search_id,
         zx_status_get_string(status));

  auto it = searches_.find(search_id);

  if (it == searches_.end() || !adapter().is_alive()) {
    return;
  }

  adapter()->bredr()->RemoveServiceSearch(it->second.search_id);

  searches_.erase(it);
}

void ProfileServer::OnServiceFound(
    uint64_t search_id, bt::PeerId peer_id,
    const std::map<bt::sdp::AttributeId, bt::sdp::DataElement>& attributes) {
  auto search_it = searches_.find(search_id);
  if (search_it == searches_.end()) {
    // Search was de-registered.
    return;
  }

  // Convert ProfileDescriptor Attribute
  auto it = attributes.find(bt::sdp::kProtocolDescriptorList);

  fidl::VectorPtr<fidlbredr::ProtocolDescriptor> descriptor_list;

  if (it != attributes.end()) {
    std::vector<fidlbredr::ProtocolDescriptor> list;
    size_t idx = 0;
    auto* sdp_list_element = it->second.At(idx);
    while (sdp_list_element != nullptr) {
      fidlbredr::ProtocolDescriptorPtr desc = DataElementToProtocolDescriptor(sdp_list_element);
      if (!desc) {
        break;
      }
      list.push_back(std::move(*desc));
      sdp_list_element = it->second.At(++idx);
    }
    descriptor_list = std::move(list);
  }

  // Add the rest of the attributes
  std::vector<fidlbredr::Attribute> fidl_attrs;

  for (const auto& it : attributes) {
    auto attr = std::make_unique<fidlbredr::Attribute>();
    attr->id = it.first;
    attr->element = std::move(*DataElementToFidl(&it.second));
    fidl_attrs.emplace_back(std::move(*attr));
  }

  fuchsia::bluetooth::PeerId fidl_peer_id{peer_id.value()};

  search_it->second.results->ServiceFound(fidl_peer_id, std::move(descriptor_list),
                                          std::move(fidl_attrs), []() {});
}

void ProfileServer::OnScoConnectionResult(
    fbl::RefPtr<ScoRequest> request,  // NOLINT(performance-unnecessary-value-param)
    bt::sco::ScoConnectionManager::AcceptConnectionResult result) {
  auto receiver = std::move(request->receiver);

  if (result.is_error()) {
    if (!receiver.is_bound()) {
      return;
    }

    bt_log(INFO, "fidl", "%s: SCO connection failed (status: %s)", __FUNCTION__,
           bt::HostErrorToString(result.error_value()).c_str());

    fidlbredr::ScoErrorCode fidl_error = fidlbredr::ScoErrorCode::FAILURE;
    if (result.error_value() == bt::HostError::kCanceled) {
      fidl_error = fidlbredr::ScoErrorCode::CANCELLED;
    }
    if (result.error_value() == bt::HostError::kParametersRejected) {
      fidl_error = fidlbredr::ScoErrorCode::PARAMETERS_REJECTED;
    }
    receiver->Error(fidl_error);
    return;
  }

  bt::sco::ScoConnection::WeakPtr connection = std::move(result.value().first);
  const uint16_t max_tx_data_size = connection->max_tx_sdu_size();

  fidl::InterfaceHandle<fidlbredr::ScoConnection> sco_handle;

  std::unique_ptr<ScoConnectionServer> sco_server =
      std::make_unique<ScoConnectionServer>(sco_handle.NewRequest(), connection);
  auto [server_iter, _] = sco_connection_servers_.emplace(&connection.get(), std::move(sco_server));

  // Activate after adding the connection to the map in case on_closed is called synchronously.
  auto on_closed = [this, conn = &connection.get()] {
    auto iter = sco_connection_servers_.find(conn);
    BT_ASSERT(iter != sco_connection_servers_.end());
    sco_connection_servers_.erase(iter);
  };
  server_iter->second->Activate(std::move(on_closed));

  if (!receiver.is_bound()) {
    return;
  }

  size_t parameter_index = result.value().second;
  BT_ASSERT_MSG(parameter_index < request->parameters.size(),
                "parameter_index (%zu)  >= request->parameters.size() (%zu)", parameter_index,
                request->parameters.size());
  fidlbredr::ScoConnectionParameters parameters = std::move(request->parameters[parameter_index]);
  parameters.set_max_tx_data_size(max_tx_data_size);
  receiver->Connected(std::move(sco_handle), std::move(parameters));
}

void ProfileServer::OnAudioDirectionExtError(AudioDirectionExt* ext_server, zx_status_t status) {
  bt_log(DEBUG, "fidl", "audio direction ext server closed (reason: %s)",
         zx_status_get_string(status));
  auto handle = audio_direction_ext_servers_.extract(ext_server->unique_id());
  if (handle.empty()) {
    bt_log(WARN, "fidl", "could not find ext server in audio direction ext error callback");
  }
}

fidl::InterfaceHandle<fidlbredr::AudioDirectionExt> ProfileServer::BindAudioDirectionExtServer(
    bt::l2cap::Channel::WeakPtr channel) {
  fidl::InterfaceHandle<fidlbredr::AudioDirectionExt> client;

  bt::l2cap::Channel::UniqueId unique_id = channel->unique_id();

  auto audio_direction_ext_server =
      std::make_unique<AudioDirectionExt>(client.NewRequest(), std::move(channel));
  AudioDirectionExt* server_ptr = audio_direction_ext_server.get();

  audio_direction_ext_server->set_error_handler(
      [this, server_ptr](zx_status_t status) { OnAudioDirectionExtError(server_ptr, status); });

  audio_direction_ext_servers_[unique_id] = std::move(audio_direction_ext_server);

  return client;
}

void ProfileServer::OnL2capParametersExtError(L2capParametersExt* ext_server, zx_status_t status) {
  bt_log(DEBUG, "fidl", "fidl parameters ext server closed (reason: %s)",
         zx_status_get_string(status));
  auto handle = l2cap_parameters_ext_servers_.extract(ext_server->unique_id());
  if (handle.empty()) {
    bt_log(WARN, "fidl", "could not find ext server in l2cap parameters ext error callback");
  }
}

fidl::InterfaceHandle<fidlbredr::L2capParametersExt> ProfileServer::BindL2capParametersExtServer(
    bt::l2cap::Channel::WeakPtr channel) {
  fidl::InterfaceHandle<fidlbredr::L2capParametersExt> client;

  bt::l2cap::Channel::UniqueId unique_id = channel->unique_id();

  auto l2cap_parameters_ext_server =
      std::make_unique<L2capParametersExt>(client.NewRequest(), std::move(channel));
  L2capParametersExt* server_ptr = l2cap_parameters_ext_server.get();

  l2cap_parameters_ext_server->set_error_handler(
      [this, server_ptr](zx_status_t status) { OnL2capParametersExtError(server_ptr, status); });

  l2cap_parameters_ext_servers_[unique_id] = std::move(l2cap_parameters_ext_server);
  return client;
}

void ProfileServer::OnAudioOffloadExtError(AudioOffloadExt* ext_server, zx_status_t status) {
  bt_log(DEBUG, "fidl", "audio offload ext server closed (reason: %s)",
         zx_status_get_string(status));
  auto handle = audio_offload_ext_servers_.extract(ext_server);
  if (handle.empty()) {
    bt_log(WARN, "fidl", "could not find ext server in audio offload ext error callback");
  }
}

fidl::InterfaceHandle<fidlbredr::AudioOffloadExt> ProfileServer::BindAudioOffloadExtServer(
    bt::l2cap::Channel::WeakPtr channel) {
  fidl::InterfaceHandle<fidlbredr::AudioOffloadExt> client;

  std::unique_ptr<bthost::ProfileServer::AudioOffloadExt> audio_offload_ext_server =
      std::make_unique<AudioOffloadExt>(*this, client.NewRequest(), std::move(channel), adapter_);
  AudioOffloadExt* server_ptr = audio_offload_ext_server.get();

  audio_offload_ext_server->set_error_handler(
      [this, server_ptr](zx_status_t status) { OnAudioOffloadExtError(server_ptr, status); });

  // TODO(fxbug.dev/126635): Use unique channel id instead of server_ptr
  audio_offload_ext_servers_[server_ptr] = std::move(audio_offload_ext_server);
  return client;
}

fuchsia::bluetooth::bredr::Channel ProfileServer::ChannelToFidl(
    bt::l2cap::Channel::WeakPtr channel) {
  BT_ASSERT(channel.is_alive());
  fidlbredr::Channel fidl_chan;
  fidl_chan.set_channel_mode(ChannelModeToFidl(channel->mode()));
  fidl_chan.set_max_tx_sdu_size(channel->max_tx_sdu_size());
  if (channel->info().flush_timeout) {
    fidl_chan.set_flush_timeout(channel->info().flush_timeout->get());
  }

  if (adapter()->state().IsControllerFeatureSupported(FeaturesBits::kSetAclPriorityCommand)) {
    fidl_chan.set_ext_direction(BindAudioDirectionExtServer(channel));
  }

  if (adapter()->state().IsControllerFeatureSupported(FeaturesBits::kAndroidVendorExtensions) &&
      adapter()->state().android_vendor_capabilities.a2dp_source_offload_capability_mask()) {
    fidl_chan.set_ext_audio_offload(BindAudioOffloadExtServer(channel));
  }

  fidl_chan.set_ext_l2cap(BindL2capParametersExtServer(channel));

  auto closed_cb = [this, unique_id = channel->unique_id()]() {
    l2cap_parameters_ext_servers_.erase(unique_id);
    audio_direction_ext_servers_.erase(unique_id);
    audio_offload_controller_server_ = nullptr;
    // TODO(fxbug.dev/113355): Erase other channel extension servers.
  };

  auto sock = l2cap_socket_factory_.MakeSocketForChannel(std::move(channel), std::move(closed_cb));
  fidl_chan.set_socket(std::move(sock));
  return fidl_chan;
}

void ProfileServer::AudioDirectionExt::SetPriority(
    fuchsia::bluetooth::bredr::A2dpDirectionPriority priority, SetPriorityCallback callback) {
  channel_->RequestAclPriority(FidlToAclPriority(priority),
                               [cb = std::move(callback)](auto result) {
                                 if (result.is_ok()) {
                                   cb(fpromise::ok());
                                   return;
                                 }
                                 bt_log(DEBUG, "fidl", "ACL priority request failed");
                                 cb(fpromise::error(fuchsia::bluetooth::ErrorCode::FAILED));
                               });
}

}  // namespace bthost
