// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_PROFILE_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_PROFILE_SERVER_H_

#include <fuchsia/bluetooth/bredr/cpp/fidl.h>

#include <fbl/macros.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/server.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/service_record.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Implements the bredr::Profile FIDL interface.
class ProfileServer : public ServerBase<fuchsia::bluetooth::bredr::Profile> {
 public:
  ProfileServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                fidl::InterfaceRequest<fuchsia::bluetooth::bredr::Profile> request);
  ~ProfileServer() override;

 private:
  class AudioDirectionExt;

  // fuchsia::bluetooth::bredr::Profile overrides:
  void Advertise(std::vector<fuchsia::bluetooth::bredr::ServiceDefinition> definitions,
                 fuchsia::bluetooth::bredr::ChannelParameters parameters,
                 fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver,
                 AdvertiseCallback callback) override;
  void Search(fuchsia::bluetooth::bredr::ServiceClassProfileIdentifier service_uuid,
              std::vector<uint16_t> attr_ids,
              fidl::InterfaceHandle<fuchsia::bluetooth::bredr::SearchResults> results) override;
  void Connect(fuchsia::bluetooth::PeerId peer_id,
               fuchsia::bluetooth::bredr::ConnectParameters connection,
               ConnectCallback callback) override;

  // Callback when clients close their connection targets
  void OnConnectionReceiverError(uint64_t ad_id, zx_status_t status);

  // Callback when clients close their search results
  void OnSearchResultError(uint64_t search_id, zx_status_t status);

  // Callback for incoming connections
  void OnChannelConnected(uint64_t ad_id, bt::l2cap::ChannelSocket chan_sock,
                          bt::hci::ConnectionHandle handle,
                          const bt::sdp::DataElement& protocol_list);

  // Callback for services found on connected device
  void OnServiceFound(uint64_t search_id, bt::PeerId peer_id,
                      const std::map<bt::sdp::AttributeId, bt::sdp::DataElement>& attributes);

  // Callback for a channel priority request.
  void OnSetPriority(bt::hci::ConnectionHandle handle,
                     fuchsia::bluetooth::bredr::A2dpDirectionPriority priority,
                     fuchsia::bluetooth::bredr::AudioDirectionExt::SetPriorityCallback cb);

  // Callback when clients close their audio direction extension.
  void OnAudioDirectionExtError(AudioDirectionExt* ext_server, bt::hci::ConnectionHandle handle,
                                zx_status_t status);

  // Create an AudioDirectionExt server for the given connection |handle| and set up callbacks.
  // Returns the client end of the channel.
  fidl::InterfaceHandle<fuchsia::bluetooth::bredr::AudioDirectionExt> BindAudioDirectionExtServer(
      bt::hci::ConnectionHandle handle);

  bt::gap::Adapter* adapter() const { return adapter_.get(); }

  // Advertised Services
  struct AdvertisedService {
    AdvertisedService(fidl::InterfacePtr<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver,
                      bt::sdp::Server::RegistrationHandle registration_handle,
                      AdvertiseCallback disconnection_cb)
        : receiver(std::move(receiver)),
          registration_handle(registration_handle),
          disconnection_cb(std::move(disconnection_cb)) {}
    fidl::InterfacePtr<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver;
    bt::sdp::Server::RegistrationHandle registration_handle;
    AdvertiseCallback disconnection_cb;
  };

  uint64_t advertised_total_;
  std::map<uint64_t, AdvertisedService> current_advertised_;

  // Searches registered
  struct RegisteredSearch {
    RegisteredSearch(fidl::InterfacePtr<fuchsia::bluetooth::bredr::SearchResults> results,
                     bt::gap::BrEdrConnectionManager::SearchId search_id)
        : results(std::move(results)), search_id(search_id) {}
    fidl::InterfacePtr<fuchsia::bluetooth::bredr::SearchResults> results;
    bt::gap::BrEdrConnectionManager::SearchId search_id;
  };

  uint64_t searches_total_;
  std::map<uint64_t, RegisteredSearch> searches_;

  class AudioDirectionExt : public ServerBase<fuchsia::bluetooth::bredr::AudioDirectionExt> {
   public:
    // Calls to SetPriority() are forwarded to |priority_cb|.
    AudioDirectionExt(
        fidl::InterfaceRequest<fuchsia::bluetooth::bredr::AudioDirectionExt> request,
        fit::function<void(fuchsia::bluetooth::bredr::A2dpDirectionPriority, SetPriorityCallback)>
            priority_cb);

    // fuchsia::bluetooth::bredr::AudioDirectionExt overrides:
    void SetPriority(fuchsia::bluetooth::bredr::A2dpDirectionPriority priority,
                     SetPriorityCallback callback) override;

    fuchsia::bluetooth::bredr::A2dpDirectionPriority priority() const { return priority_; }

   private:
    fuchsia::bluetooth::bredr::A2dpDirectionPriority priority_;
    fit::function<void(fuchsia::bluetooth::bredr::A2dpDirectionPriority, SetPriorityCallback)> cb_;
  };
  std::unordered_map<AudioDirectionExt*, std::unique_ptr<AudioDirectionExt>>
      audio_direction_ext_servers_;

  fxl::WeakPtr<bt::gap::Adapter> adapter_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<ProfileServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ProfileServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_PROFILE_SERVER_H_
