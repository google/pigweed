// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_TRANSPORT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_TRANSPORT_H_

#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>

#include <atomic>
#include <memory>
#include <thread>

#include "pw_bluetooth/controller.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/weak_self.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"

namespace bt::hci {

// Represents the HCI transport layer. This object owns the HCI command, ACL,
// and SCO channels and provides the necessary control-flow mechanisms to send
// and receive HCI packets from the underlying Bluetooth controller.
class Transport final : public WeakSelf<Transport> {
 public:
  explicit Transport(std::unique_ptr<pw::bluetooth::Controller> hci);

  // Initializes the command channel and features. The result will be reported via
  // |complete_callback|.
  //
  // NOTE: AclDataChannel and ScoDataChannel will be left uninitialized. They must be
  // initialized after available data buffer information has been obtained from
  // the controller (via HCI_Read_Buffer_Size and HCI_LE_Read_Buffer_Size).
  void Initialize(fit::callback<void(bool /*success*/)> complete_callback);

  // TODO(armansito): hci::Transport::~Transport() should send a shutdown message
  // to the bt-hci device, which would be responsible for sending HCI_Reset upon
  // exit.
  ~Transport();

  // Initializes the ACL data channel with the given parameters. Returns false
  // if an error occurs during initialization. Initialize() must have been
  // called successfully prior to calling this method.
  bool InitializeACLDataChannel(const DataBufferInfo& bredr_buffer_info,
                                const DataBufferInfo& le_buffer_info);

  // Initializes the SCO data channel with the given parameters. Returns false
  // if an error occurs during initialization.
  bool InitializeScoDataChannel(const DataBufferInfo& buffer_info);

  pw::bluetooth::Controller::FeaturesBits GetFeatures();

  // Returns a pointer to the HCI command and event flow control handler.
  // CommandChannel is guaranteed to live as long as Transport, but may stop
  // processing packets after the Transport error callback has been called.
  CommandChannel* command_channel() const { return command_channel_.get(); }

  // Returns a pointer to the HCI ACL data flow control handler. Nullptr until
  // InitializeACLDataChannel() has succeeded.
  // AclDataChannel is guaranteed to live as long as Transport.
  AclDataChannel* acl_data_channel() const { return acl_data_channel_.get(); }

  // Returns a pointer to the HCI SCO data flow control handler. Nullptr until
  // InitializeScoDataChannel succeeds.
  // ScoDataChannel is guaranteed to live as long as Transport.
  ScoDataChannel* sco_data_channel() const { return sco_data_channel_.get(); }

  // Set a callback that should be invoked when any one of the underlying channels experiences a
  // fatal error (e.g. the HCI device has disappeared).
  //
  // When this callback is called the channels will be in an invalid state and packet processing is
  // no longer guaranteed to work. However, the channel pointers are guaranteed to still be valid.
  // It is the responsibility of the callback implementation to clean up this Transport instance.
  void SetTransportErrorCallback(fit::closure callback);

  // Attach hci transport inspect node as a child node of |parent|.
  static constexpr const char* kInspectNodeName = "hci";
  void AttachInspect(inspect::Node& parent, const std::string& name = kInspectNodeName);

 private:
  // Callback called by CommandChannel or ACLDataChannel on errors.
  void OnChannelError();

  // HCI inspect node.
  inspect::Node hci_node_;

  // Callback invoked when the transport is closed (due to a channel error).
  fit::closure error_cb_;

  std::unique_ptr<pw::bluetooth::Controller> controller_;

  std::optional<pw::bluetooth::Controller::FeaturesBits> features_;

  // The HCI command and event flow control handler.
  // CommandChannel must be constructed first & shut down last because AclDataChannel and
  // ScoDataChannel depend on it. CommandChannel must live as long as Transport to meet the
  // expectations of upper layers, which may try to send commands on destruction.
  std::unique_ptr<CommandChannel> command_channel_;

  // The ACL data flow control handler.
  std::unique_ptr<AclDataChannel> acl_data_channel_;

  // The SCO data flow control handler.
  std::unique_ptr<ScoDataChannel> sco_data_channel_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Transport);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_TRANSPORT_H_
