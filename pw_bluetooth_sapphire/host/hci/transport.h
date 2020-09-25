// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_TRANSPORT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_TRANSPORT_H_

#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/result.h>

#include <atomic>
#include <memory>
#include <thread>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/lib/fxl/memory/ref_counted.h"
#include "src/lib/fxl/memory/ref_ptr.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace hci {

class DeviceWrapper;

// Represents the HCI transport layer. This object owns the HCI command, ACL,
// and SCO channels and provides the necessary control-flow mechanisms to send
// and receive HCI packets from the underlying Bluetooth controller.
//
// TODO(armansito): This object has become too heavy-weight. I think it will be
// cleaner to have CommandChannel and ACLDataChannel each be owned directly by
// the main and L2CAP domains. Transport should go away as part of the HCI layer
// clean up (and also fxbug.dev/721).
class Transport final {
 public:
  // Initializes the command channel.
  //
  // NOTE: The ACLDataChannel will be left uninitialized. The ACLDataChannel must be
  // initialized after available data buffer information has been obtained from
  // the controller (via HCI_Read_Buffer_Size and HCI_LE_Read_Buffer_Size).
  static fit::result<std::unique_ptr<Transport>> Create(std::unique_ptr<DeviceWrapper> hci_device);

  // TODO(armansito): hci::Transport::~Transport() should send a shutdown message
  // to the bt-hci device, which would be responsible for sending HCI_Reset upon
  // exit.
  ~Transport();

  // Initializes the ACL data channel with the given parameters. Returns false
  // if an error occurs during initialization. Initialize() must have been
  // called successfully prior to calling this method.
  bool InitializeACLDataChannel(const DataBufferInfo& bredr_buffer_info,
                                const DataBufferInfo& le_buffer_info);

  // Returns a pointer to the HCI command and event flow control handler.
  CommandChannel* command_channel() const { return command_channel_.get(); }

  // Returns a pointer to the HCI ACL data flow control handler.
  ACLDataChannel* acl_data_channel() const { return acl_data_channel_.get(); }

  // Set a callback that should be invoked when any one of the underlying
  // channels gets closed for any reason (e.g. the HCI device has disappeared)
  // and the dispatcher on which the callback should be posted.
  //
  // When this callback is called the channels will be in an invalid state and
  // packet processing is no longer guaranteed to work. It is the responsibility
  // of the callback implementation to clean up this Transport instance by
  // calling ShutDown() and/or deleting it.
  void SetTransportClosedCallback(fit::closure callback);

  fxl::WeakPtr<Transport> WeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  explicit Transport(std::unique_ptr<DeviceWrapper> hci_device);

  // Channel closed callback.
  void OnChannelClosed(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                       const zx_packet_signal_t* signal);
  using Waiter = async::WaitMethod<Transport, &Transport::OnChannelClosed>;

  // Sets up a wait to watch for |channel| to close and calls OnChannelClosed
  void WatchChannelClosed(const zx::channel& channel, Waiter& wait);

  // Notifies the closed callback.
  void NotifyClosedCallback();

  // Callback called by CommandChannel or ACLDataChannel on errors.
  void OnChannelError();

  // Used to assert that certain public functions are only called on the
  // creation thread.
  fxl::ThreadChecker thread_checker_;

  // The Bluetooth HCI device file descriptor.
  std::unique_ptr<DeviceWrapper> hci_device_;

  // async::Waits for the command and ACL channels
  Waiter cmd_channel_wait_{this};
  Waiter acl_channel_wait_{this};

  // The ACL data flow control handler.
  std::unique_ptr<ACLDataChannel> acl_data_channel_;

  // The HCI command and event flow control handler.
  std::unique_ptr<CommandChannel> command_channel_;

  // Callback invoked when the transport is closed (due to a channel error).
  fit::closure closed_cb_;

  fxl::WeakPtrFactory<Transport> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Transport);
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_TRANSPORT_H_
