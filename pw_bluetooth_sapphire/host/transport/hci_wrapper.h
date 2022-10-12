// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_WRAPPER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_WRAPPER_H_

#include <zircon/status.h>

#include "hci_defs.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/device_wrapper.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/hci_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_packet.h"

struct bt_hci_protocol;
struct bt_vendor_protocol;

namespace bt::hci {

// HciWrapper wraps the underlying HCI API, whether it be a Banjo, FIDL, or test fixture.
// TODO(fxbug.dev/98342): Remove use of zx_status_t in this interface.
class HciWrapper {
 public:
  using ErrorCallback = fit::callback<void(zx_status_t)>;

  using AclPacketFunction = fit::function<void(std::unique_ptr<ACLDataPacket>)>;

  using EventPacketFunction = fit::function<void(std::unique_ptr<EventPacket>)>;

  using ScoPacketFunction = fit::function<void(std::unique_ptr<ScoDataPacket>)>;

  using StatusCallback = fit::callback<void(zx_status_t)>;

  // Create a production HciWrapper. All callbacks will be run on |dispatcher|.
  static std::unique_ptr<HciWrapper> Create(std::unique_ptr<DeviceWrapper> device,
                                            async_dispatcher_t* dispatcher);

  virtual ~HciWrapper() = default;

  // Starts processing channel signals and calling packet callbacks. Returns true if initialization
  // completed successfully. |error_callback| will be called with fatal errors that occur after
  // initialization (e.g. channel closure). This object will be invalid after an error is reported,
  // and should be destroyed.
  virtual bool Initialize(ErrorCallback error_callback) = 0;

  // Sends an HCI command packet and returns the status of the operation.
  [[nodiscard]] virtual zx_status_t SendCommand(std::unique_ptr<CommandPacket> packet) = 0;

  // Same as above; Emboss version.
  [[nodiscard]] virtual zx_status_t SendCommand(EmbossCommandPacket packet) = 0;

  // Sets a callback that will be called with inbound event packets.
  virtual void SetEventCallback(EventPacketFunction callback) = 0;

  // Sends an ACL packet and returns the status of the operation.
  [[nodiscard]] virtual zx_status_t SendAclPacket(std::unique_ptr<ACLDataPacket> packet) = 0;

  // Sets a callback that will be called with inbound ACL packets.
  virtual void SetAclCallback(AclPacketFunction callback) = 0;

  // Sends a SCO packet and returns the status of the operation. If SCO is not supported, an error
  // will be returned.
  [[nodiscard]] virtual zx_status_t SendScoPacket(std::unique_ptr<ScoDataPacket> packet) = 0;

  // Sets a callback that will be called with inbound SCO packets.
  virtual void SetScoCallback(ScoPacketFunction callback) = 0;

  // Returns true if SCO is supported by the transport & controller.
  virtual bool IsScoSupported() = 0;

  // Configure the HCI for a SCO connection with the indicated parameters. This must be called
  // before sending/receiving data on the SCO channel. |callback| will be called with the result of
  // the operation.
  virtual void ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding,
                            ScoSampleRate sample_rate, StatusCallback callback) = 0;

  // Releases resources held by an active SCO connection. Must be called when a SCO connection is
  // closed.
  // Returns ZX_ERR_NOT_SUPPORTED if SCO is not supported by the current vendor or transport driver.
  virtual void ResetSco(StatusCallback callback) = 0;

  // Returns bitmask of the features the vendor supports.
  virtual VendorFeaturesBits GetVendorFeatures() = 0;

  // Encodes the vendor HCI command "Set ACL Priority" and returns the encoded
  // command as a buffer on success. Returns an error if the command is not supported or
  // the parameters are invalid.
  virtual fit::result<zx_status_t, DynamicByteBuffer> EncodeSetAclPriorityCommand(
      hci_spec::ConnectionHandle connection, hci::AclPriority priority) = 0;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_WRAPPER_H_
