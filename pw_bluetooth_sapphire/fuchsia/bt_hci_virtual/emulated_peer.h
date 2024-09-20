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

#pragma once

#include <fidl/fuchsia.hardware.bluetooth/cpp/fidl.h>
#include <lib/fpromise/result.h>

#include <vector>

#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_peer.h"

namespace bt_hci_virtual {

// Responsible for processing FIDL messages to/from an emulated peer instance.
// This class is not thread-safe.
//
// When the remote end of the FIDL channel gets closed the underlying FakePeer
// will be removed from the fake controller and the |closed_callback| that is
// passed to the constructor will get notified. The owner of this object should
// act on this by destroying this Peer instance.
class EmulatedPeer : public fidl::Server<fuchsia_hardware_bluetooth::Peer> {
 public:
  using Result =
      fpromise::result<std::unique_ptr<EmulatedPeer>,
                       fuchsia_hardware_bluetooth::EmulatorPeerError>;

  // Registers a peer with the FakeController using the provided LE parameters.
  // Returns the peer on success or an error reporting the failure.
  static Result NewLowEnergy(
      fuchsia_hardware_bluetooth::PeerParameters parameters,
      bt::testing::FakeController* fake_controller,
      async_dispatcher_t* dispatcher);

  // Registers a peer with the FakeController using the provided BR/EDR
  // parameters. Returns the peer on success or an error reporting the failure.
  static Result NewBredr(fuchsia_hardware_bluetooth::PeerParameters parameters,
                         bt::testing::FakeController* fake_controller,
                         async_dispatcher_t* dispatcher);

  // The destructor unregisters the Peer if initialized.
  ~EmulatedPeer();

  // Rerturns the device address that this instance was initialized with based
  // on the FIDL parameters.
  const bt::DeviceAddress& address() const { return address_; }

  // Assign a callback that will run when the Peer handle gets closed.
  void set_closed_callback(fit::callback<void()> closed_callback) {
    closed_callback_ = std::move(closed_callback);
  }

  // fuchsia_hardware_bluetooth::Peer overrides:
  void AssignConnectionStatus(
      AssignConnectionStatusRequest& request,
      AssignConnectionStatusCompleter::Sync& completer) override;
  void EmulateLeConnectionComplete(
      EmulateLeConnectionCompleteRequest& request,
      EmulateLeConnectionCompleteCompleter::Sync& completer) override;
  void EmulateDisconnectionComplete(
      EmulateDisconnectionCompleteCompleter::Sync& completer) override;
  void WatchConnectionStates(
      WatchConnectionStatesCompleter::Sync& completer) override;
  void SetDeviceClass(SetDeviceClassRequest& request,
                      SetDeviceClassCompleter::Sync& completer) override;
  void SetServiceDefinitions(
      SetServiceDefinitionsRequest& request,
      SetServiceDefinitionsCompleter::Sync& completer) override;
  void SetLeAdvertisement(
      SetLeAdvertisementRequest& request,
      SetLeAdvertisementCompleter::Sync& completer) override;
  void handle_unknown_method(
      fidl::UnknownMethodMetadata<fuchsia_hardware_bluetooth::Peer> metadata,
      fidl::UnknownMethodCompleter::Sync& completer) override;

  // Updates this peer with the current connection state which is used to notify
  // its FIDL client of state changes that it is observing.
  void UpdateConnectionState(bool connected);
  void MaybeUpdateConnectionStates();

 private:
  EmulatedPeer(bt::DeviceAddress address,
               fidl::ServerEnd<fuchsia_hardware_bluetooth::Peer> request,
               bt::testing::FakeController* fake_controller,
               async_dispatcher_t* dispatcher);

  void OnChannelClosed(fidl::UnbindInfo info);
  void CleanUp();
  void NotifyChannelClosed();

  bt::DeviceAddress address_;
  bt::testing::FakeController* fake_controller_;
  fidl::ServerBinding<fuchsia_hardware_bluetooth::Peer> binding_;
  fit::callback<void()> closed_callback_;

  std::mutex connection_states_lock_;
  std::vector<fuchsia_hardware_bluetooth::ConnectionState> connection_states_;
  std::queue<WatchConnectionStatesCompleter::Async>
      connection_states_completers_ __TA_GUARDED(connection_states_lock_);
};

}  // namespace bt_hci_virtual
