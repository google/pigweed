// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include <zircon/status.h>

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/status.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;

namespace fidlbredr = fuchsia::bluetooth::bredr;
using fidlbredr::DataElement;
using fidlbredr::Profile;

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
        ZX_PANIC("FIDL channel parameter contains invalid mode");
    }
  }
  if (fidl.has_max_rx_sdu_size()) {
    params.max_rx_sdu_size = fidl.max_rx_sdu_size();
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
      ZX_PANIC("Could not convert channel parameter mode to unsupported FIDL mode");
  }
}

fidlbredr::Channel ChannelSocketToFidlChannel(bt::l2cap::ChannelSocket chan_sock) {
  fidlbredr::Channel chan;
  if (!chan_sock) {
    return chan;
  }
  chan.set_socket(std::move(chan_sock.socket));
  chan.set_channel_mode(ChannelModeToFidl(chan_sock.params->mode));
  chan.set_max_tx_sdu_size(chan_sock.params->max_tx_sdu_size);
  return chan;
}

fidlbredr::DataElementPtr DataElementToFidl(const bt::sdp::DataElement* in) {
  auto elem = fidlbredr::DataElement::New();
  bt_log(TRACE, "sdp", "DataElementToFidl: %s", in->ToString().c_str());
  ZX_DEBUG_ASSERT(in);
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
          bt_log(INFO, "profile_server", "no 128-bit integer support in FIDL yet");
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
          bt_log(INFO, "profile_server", "no 128-bit integer support in FIDL yet");
          return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kUuid: {
      auto uuid = in->Get<bt::UUID>();
      ZX_DEBUG_ASSERT(uuid);
      elem->set_uuid(fidl_helpers::UuidToFidl(*uuid));
      return elem;
    }
    case bt::sdp::DataElement::Type::kString: {
      elem->set_str(*in->Get<std::string>());
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
      bt_log(INFO, "profile_server", "no support for Url types in DataElement yet");
      return nullptr;
    }
    case bt::sdp::DataElement::Type::kNull: {
      bt_log(INFO, "profile_server", "no support for null DataElement types in FIDL");
      return nullptr;
    }
  }
}

fidlbredr::ProtocolDescriptorPtr DataElementToProtocolDescriptor(const bt::sdp::DataElement* in) {
  auto desc = fidlbredr::ProtocolDescriptor::New();
  if (in->type() != bt::sdp::DataElement::Type::kSequence) {
    return nullptr;
  }
  const auto protocol_uuid = in->At(0)->Get<bt::UUID>();
  if (!protocol_uuid) {
    return nullptr;
  }
  desc->protocol = fidlbredr::ProtocolIdentifier(*protocol_uuid->As16Bit());
  const bt::sdp::DataElement* it;
  for (size_t idx = 1; (it = in->At(idx)); ++idx) {
    desc->params.push_back(std::move(*DataElementToFidl(it)));
  }

  return desc;
}
}  // namespace

ProfileServer::ProfileServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                             fidl::InterfaceRequest<Profile> request)
    : ServerBase(this, std::move(request)),
      advertised_total_(0),
      searches_total_(0),
      adapter_(adapter),
      weak_ptr_factory_(this) {}

ProfileServer::~ProfileServer() {
  if (adapter()) {
    // Unregister anything that we have registered.
    auto sdp = adapter()->sdp_server();
    for (const auto& it : current_advertised_) {
      sdp->UnregisterService(it.second.registration_handle);
    }
    auto conn_manager = adapter()->bredr_connection_manager();
    for (const auto& it : searches_) {
      conn_manager->RemoveServiceSearch(it.second.search_id);
    }
  }
}

void ProfileServer::Advertise(
    std::vector<fidlbredr::ServiceDefinition> definitions, fidlbredr::ChannelParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver) {
  // TODO: check that the service definition is valid for useful error messages

  std::vector<bt::sdp::ServiceRecord> registering;

  for (auto& definition : definitions) {
    auto rec = fidl_helpers::ServiceDefinitionToServiceRecord(definition);
    // Drop the receiver on error.
    if (rec.is_error()) {
      bt_log(INFO, "profile_server",
             "Failed to create service record from service defintion - exiting!");
      return;
    }
    registering.emplace_back(std::move(rec.value()));
  }

  ZX_DEBUG_ASSERT(adapter());
  auto sdp = adapter()->sdp_server();
  ZX_DEBUG_ASSERT(sdp);

  uint64_t next = advertised_total_ + 1;

  auto registration_handle = sdp->RegisterService(
      std::move(registering), FidlToChannelParameters(parameters),
      [this, next](auto chan_sock, auto handle, const auto& protocol_list) {
        OnChannelConnected(next, std::move(chan_sock), handle, std::move(protocol_list));
      });

  if (!registration_handle) {
    return;
  };

  auto receiverptr = receiver.Bind();

  receiverptr.set_error_handler(
      [this, next](zx_status_t status) { OnConnectionReceiverError(next, status); });

  current_advertised_.try_emplace(next, std::move(receiverptr), registration_handle);
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

  ZX_DEBUG_ASSERT(adapter());

  auto next = searches_total_ + 1;

  auto search_id = adapter()->bredr_connection_manager()->AddServiceSearch(
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
    callback(fit::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS));
    return;
  }

  // The L2CAP parameters must include a PSM. ChannelParameters are optional.
  auto l2cap_params = std::move(connection.l2cap());
  if (!l2cap_params.has_psm()) {
    callback(fit::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS));
    return;
  }
  uint16_t psm = l2cap_params.psm();

  fidlbredr::ChannelParameters parameters = std::move(*l2cap_params.mutable_parameters());

  auto connected_cb = [self = weak_ptr_factory_.GetWeakPtr(),
                       cb = callback.share()](auto chan_sock) {
    if (!chan_sock) {
      bt_log(TRACE, "profile_server", "Channel socket is empty, returning failed.");
      cb(fit::error(fuchsia::bluetooth::ErrorCode::FAILED));
      return;
    }

    if (!self) {
      cb(fit::error(fuchsia::bluetooth::ErrorCode::FAILED));
      return;
    }

    ZX_ASSERT(chan_sock.params->handle.has_value());
    auto handle = chan_sock.params->handle.value();
    auto audio_direction_ext_client = self->BindAudioDirectionExtServer(handle);

    auto chan = ChannelSocketToFidlChannel(std::move(chan_sock));
    chan.set_ext_direction(std::move(audio_direction_ext_client));

    cb(fit::ok(std::move(chan)));
  };
  ZX_DEBUG_ASSERT(adapter());

  bool connecting = adapter()->bredr_connection_manager()->OpenL2capChannel(
      id, psm, fidl_helpers::FidlToBrEdrSecurityRequirements(parameters),
      FidlToChannelParameters(parameters), std::move(connected_cb));
  if (!connecting) {
    callback(fit::error(fuchsia::bluetooth::ErrorCode::NOT_FOUND));
  }
}

void ProfileServer::OnChannelConnected(uint64_t ad_id, bt::l2cap::ChannelSocket chan_sock,
                                       bt::hci::ConnectionHandle handle,
                                       const bt::sdp::DataElement& protocol_list) {
  auto it = current_advertised_.find(ad_id);
  if (it == current_advertised_.end()) {
    // The receiver has disappeared, do nothing.
    return;
  }

  ZX_DEBUG_ASSERT(adapter());
  auto id = adapter()->bredr_connection_manager()->GetPeerId(handle);

  // The protocol that is connected should be L2CAP, because that is the only thing that
  // we can connect. We can't say anything about what the higher level protocols will be.
  auto prot_seq = protocol_list.At(0);
  ZX_ASSERT(prot_seq);

  fidlbredr::ProtocolDescriptorPtr desc = DataElementToProtocolDescriptor(prot_seq);
  ZX_ASSERT(desc);

  fuchsia::bluetooth::PeerId peer_id{id.value()};

  std::vector<fidlbredr::ProtocolDescriptor> list;
  list.emplace_back(std::move(*desc));

  auto audio_direction_ext_client = BindAudioDirectionExtServer(handle);

  auto chan = ChannelSocketToFidlChannel(std::move(chan_sock));
  chan.set_ext_direction(std::move(audio_direction_ext_client));

  it->second.receiver->Connected(peer_id, std::move(chan), std::move(list));
}

void ProfileServer::OnConnectionReceiverError(uint64_t ad_id, zx_status_t status) {
  bt_log(TRACE, "profile_server", "Connection receiver closed, ending advertisement %lu", ad_id);

  auto it = current_advertised_.find(ad_id);

  if (it == current_advertised_.end() || !adapter()) {
    return;
  }

  adapter()->sdp_server()->UnregisterService(it->second.registration_handle);

  current_advertised_.erase(it);
}

void ProfileServer::OnSearchResultError(uint64_t search_id, zx_status_t status) {
  bt_log(TRACE, "profile_server", "Search result closed, ending search %lu reason %s", search_id,
         zx_status_get_string(status));

  auto it = searches_.find(search_id);

  if (it == searches_.end() || !adapter()) {
    return;
  }

  adapter()->bredr_connection_manager()->RemoveServiceSearch(it->second.search_id);

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
    auto attr = fidlbredr::Attribute::New();
    attr->id = it.first;
    attr->element = std::move(*DataElementToFidl(&it.second));
    fidl_attrs.emplace_back(std::move(*attr));
  }

  fuchsia::bluetooth::PeerId fidl_peer_id{peer_id.value()};

  search_it->second.results->ServiceFound(fidl_peer_id, std::move(descriptor_list),
                                          std::move(fidl_attrs), []() {});
}

void ProfileServer::OnSetPriority(bt::hci::ConnectionHandle handle,
                                  fidlbredr::A2dpDirectionPriority priority,
                                  fidlbredr::AudioDirectionExt::SetPriorityCallback cb) {
  auto packet = bt::hci::CommandPacket::New(bt::hci::kBcmSetAclPriority,
                                            sizeof(bt::hci::BcmSetAclPriorityCommandParams));
  auto params = packet->mutable_payload<bt::hci::BcmSetAclPriorityCommandParams>();
  params->handle = htole16(handle);
  params->priority = bt::hci::BcmAclPriority::kHigh;
  params->direction = bt::hci::BcmAclPriorityDirection::kSink;
  if (priority == fidlbredr::A2dpDirectionPriority::SOURCE) {
    params->direction = bt::hci::BcmAclPriorityDirection::kSource;
  } else if (priority == fidlbredr::A2dpDirectionPriority::NORMAL) {
    params->priority = bt::hci::BcmAclPriority::kNormal;
  }

  // TODO(57163): Return error if there is a priority conflict or the controller does not support
  // the ACL priority command.
  adapter()->transport()->command_channel()->SendCommand(
      std::move(packet),
      [cb = std::move(cb), priority](auto id, const bt::hci::EventPacket& event) {
        if (hci_is_error(event, WARN, "profile_server", "BCM acl priority failed")) {
          cb(fit::error(fuchsia::bluetooth::ErrorCode::FAILED));
          return;
        }

        bt_log(DEBUG, "profile_server", "BCM acl priority updated (priority: %#.8x)",
               static_cast<uint32_t>(priority));
        cb(fit::ok());
      });
}

void ProfileServer::OnAudioDirectionExtError(AudioDirectionExt* ext_server,
                                             bt::hci::ConnectionHandle handle, zx_status_t status) {
  bt_log(TRACE, "profile_server", "audio direction ext server closed (reason: %s)",
         zx_status_get_string(status));

  auto it = audio_direction_ext_servers_.find(ext_server);
  if (it == audio_direction_ext_servers_.end()) {
    bt_log(WARN, "profile_server",
           "could not find ext server in audio direction ext error callback");
    return;
  }

  // Revert any change made to ACL priority by this extension.
  // TODO(57163): Remove this hack and revert priority when a channel is closed and
  // no other channel is using the current priority.
  if (ext_server->priority() != fidlbredr::A2dpDirectionPriority::NORMAL) {
    OnSetPriority(handle, fidlbredr::A2dpDirectionPriority::NORMAL, [](auto result) {});
  }

  audio_direction_ext_servers_.erase(it);
}

fidl::InterfaceHandle<fidlbredr::AudioDirectionExt> ProfileServer::BindAudioDirectionExtServer(
    bt::hci::ConnectionHandle handle) {
  fidl::InterfaceHandle<fidlbredr::AudioDirectionExt> client;

  auto set_priority_cb = [this, handle](auto priority, auto cb) {
    OnSetPriority(handle, priority, std::move(cb));
  };

  auto audio_direction_ext_server =
      std::make_unique<AudioDirectionExt>(client.NewRequest(), std::move(set_priority_cb));
  AudioDirectionExt* server_ptr = audio_direction_ext_server.get();

  audio_direction_ext_server->set_error_handler([this, handle, server_ptr](zx_status_t status) {
    OnAudioDirectionExtError(server_ptr, handle, status);
  });

  audio_direction_ext_servers_[server_ptr] = std::move(audio_direction_ext_server);

  return client;
}

ProfileServer::AudioDirectionExt::AudioDirectionExt(
    fidl::InterfaceRequest<fidlbredr::AudioDirectionExt> request,
    fit::function<void(fidlbredr::A2dpDirectionPriority, SetPriorityCallback)> cb)
    : ServerBase(this, std::move(request)),
      priority_(fidlbredr::A2dpDirectionPriority::NORMAL),
      cb_(std::move(cb)) {}

void ProfileServer::AudioDirectionExt::SetPriority(
    fuchsia::bluetooth::bredr::A2dpDirectionPriority priority, SetPriorityCallback callback) {
  priority_ = priority;
  cb_(priority, std::move(callback));
}

}  // namespace bthost
