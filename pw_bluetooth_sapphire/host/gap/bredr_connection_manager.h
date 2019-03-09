// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_

#include "lib/fxl/memory/weak_ptr.h"

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_interrogator.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"

namespace btlib {

namespace hci {
class SequentialCommandRunner;
class Transport;
}  // namespace hci

namespace gap {

class PairingDelegate;
class RemoteDeviceCache;

// Manages all activity related to connections in the BR/EDR section of the
// controller, including whether the device can be connected to, incoming
// connections, and initiating connections.
class BrEdrConnectionManager final {
 public:
  BrEdrConnectionManager(fxl::RefPtr<hci::Transport> hci,
                         RemoteDeviceCache* device_cache,
                         fbl::RefPtr<data::Domain> data_domain,
                         bool use_interlaced_scan);
  ~BrEdrConnectionManager();

  // Set whether this host is connectable
  void SetConnectable(bool connectable, hci::StatusCallback status_cb);

  // Assigns a new PairingDelegate to handle BR/EDR authentication challenges.
  // Replacing an existing pairing delegate cancels all ongoing pairing
  // procedures. If a delegate is not set then all pairing requests will be
  // rejected.
  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate);

  // Retrieves the device id that is connected to the connection |handle|.
  // Returns common::kInvalidDeviceId if no such device exists.
  DeviceId GetPeerId(hci::ConnectionHandle handle) const;

  // Opens a new L2CAP channel to the already-connected |device_id| on psm
  // |psm|.  Returns false if the device is not already connected.
  using SocketCallback = fit::function<void(zx::socket)>;
  bool OpenL2capChannel(DeviceId device_id, l2cap::PSM psm, SocketCallback cb,
                        async_dispatcher_t* dispatcher);

 private:
  // Reads the controller page scan settings.
  void ReadPageScanSettings();

  // Writes page scan parameters to the controller.
  // If |interlaced| is true, and the controller does not support interlaced
  // page scan mode, standard mode is used.
  void WritePageScanSettings(uint16_t interval, uint16_t window,
                             bool interlaced, hci::StatusCallback cb);

  // Helper to register an event handler to run.
  hci::CommandChannel::EventHandlerId AddEventHandler(
      const hci::EventCode& code, hci::CommandChannel::EventCallback cb);

  // Callbacks for registered events
  void OnConnectionRequest(const hci::EventPacket& event);
  void OnConnectionComplete(const hci::EventPacket& event);
  void OnDisconnectionComplete(const hci::EventPacket& event);
  void OnLinkKeyRequest(const hci::EventPacket& event);
  void OnLinkKeyNotification(const hci::EventPacket& event);
  void OnIOCapabilitiesRequest(const hci::EventPacket& event);
  void OnUserConfirmationRequest(const hci::EventPacket& event);

  fxl::RefPtr<hci::Transport> hci_;
  std::unique_ptr<hci::SequentialCommandRunner> hci_cmd_runner_;

  // Device cache is used to look up parameters for connecting to devices and
  // update the state of connected devices as well as introduce unknown devices.
  // This object must outlive this instance.
  RemoteDeviceCache* cache_;

  fbl::RefPtr<data::Domain> data_domain_;

  // Interregator for new connections to pass.
  BrEdrInterrogator interrogator_;

  // Holds the connections that are active.
  std::unordered_map<hci::ConnectionHandle, hci::ConnectionPtr> connections_;

  // Handler ID for connection events
  hci::CommandChannel::EventHandlerId conn_complete_handler_id_;
  hci::CommandChannel::EventHandlerId conn_request_handler_id_;
  hci::CommandChannel::EventHandlerId disconn_cmpl_handler_id_;

  // Handler IDs for pairing events
  hci::CommandChannel::EventHandlerId link_key_request_handler_id_;
  hci::CommandChannel::EventHandlerId link_key_notification_handler_id_;
  hci::CommandChannel::EventHandlerId io_cap_req_handler_id_;
  hci::CommandChannel::EventHandlerId user_conf_handler_id_;

  // The current page scan parameters of the controller.
  // Set to 0 when non-connectable.
  uint16_t page_scan_interval_;
  uint16_t page_scan_window_;
  hci::PageScanType page_scan_type_;
  bool use_interlaced_scan_;

  // The dispatcher that all commands are queued on.
  async_dispatcher_t* dispatcher_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrConnectionManager> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(BrEdrConnectionManager);
};

}  // namespace gap
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
