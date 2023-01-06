// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_DOUBLE_BASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_DOUBLE_BASE_H_

#include <lib/async/cpp/wait.h>
#include <lib/zx/channel.h>
#include <zircon/device/bt-hci.h>

#include "pw_bluetooth/controller.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/common/weak_self.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_packet.h"

namespace bt::testing {

// Abstract base for implementing a fake HCI controller endpoint. This can
// directly send ACL data and event packets on request and forward outgoing ACL
// data packets to subclass implementations.
class ControllerTestDoubleBase : public pw::bluetooth::Controller {
 public:
  using PwStatusCallback = pw::Callback<void(pw::Status)>;

  using EncodeVendorCommandFunction =
      fit::function<void(pw::bluetooth::VendorCommandParameters,
                         pw::Callback<void(pw::Result<pw::span<const std::byte>>)>)>;

  using ConfigureScoFunction = fit::function<void(ScoCodingFormat, ScoEncoding, ScoSampleRate,
                                                  fit::callback<void(pw::Status)>)>;

  using ResetScoFunction = fit::function<void(fit::callback<void(pw::Status)>)>;

  ControllerTestDoubleBase();
  ~ControllerTestDoubleBase() override;

  // Sends the given packet over this FakeController's command channel endpoint.
  // Returns the result of the write operation on the command channel.
  zx_status_t SendCommandChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this FakeController's ACL data channel
  // endpoint.
  // Returns the result of the write operation on the channel.
  zx_status_t SendACLDataChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this ControllerTestDouble's SCO data channel
  // endpoint.
  // Returns the result of the write operation on the channel.
  zx_status_t SendScoDataChannelPacket(const ByteBuffer& packet);

  // Wrapper around SignalError() to support old test code.
  void Stop(zx_status_t = ZX_ERR_PEER_CLOSED) { SignalError(pw::Status::Aborted()); }

  void SignalError(pw::Status status) {
    if (error_cb_) {
      error_cb_(status);
    }
  }

  // This only has an effect *before* Transport has been initialized.
  void set_features(FeaturesBits features) { features_ = features; }

  void set_encode_vendor_command_cb(EncodeVendorCommandFunction cb) {
    encode_vendor_command_cb_ = std::move(cb);
  }

  void set_configure_sco_cb(ConfigureScoFunction cb) { configure_sco_cb_ = std::move(cb); }

  void set_reset_sco_cb(ResetScoFunction cb) { reset_sco_cb_ = std::move(cb); }

  // Controller overrides:
  void SetEventFunction(DataFunction func) override { event_cb_ = std::move(func); }

  void SetReceiveAclFunction(DataFunction func) override { acl_cb_ = std::move(func); }

  void SetReceiveScoFunction(DataFunction func) override { sco_cb_ = std::move(func); }

  void Initialize(PwStatusCallback complete_callback, PwStatusCallback error_callback) override;

  void Close(PwStatusCallback callback) override;

  void ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding, ScoSampleRate sample_rate,
                    pw::Callback<void(pw::Status)> callback) override;

  void ResetSco(pw::Callback<void(pw::Status)> callback) override;

  void GetFeatures(pw::Callback<void(FeaturesBits)> callback) override { callback(features_); }

  void EncodeVendorCommand(
      pw::bluetooth::VendorCommandParameters parameters,
      pw::Callback<void(pw::Result<pw::span<const std::byte>>)> callback) override {
    if (encode_vendor_command_cb_) {
      encode_vendor_command_cb_(parameters, std::move(callback));
    }
  }

 private:
  FeaturesBits features_{0};
  EncodeVendorCommandFunction encode_vendor_command_cb_;
  ConfigureScoFunction configure_sco_cb_;
  ResetScoFunction reset_sco_cb_;

  // Send inbound packets to the host stack:
  fit::function<void(pw::span<const std::byte>)> event_cb_;
  DataFunction acl_cb_;
  DataFunction sco_cb_;

  PwStatusCallback error_cb_;

  WeakSelf<ControllerTestDoubleBase> weak_self_{this};
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ControllerTestDoubleBase);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_DOUBLE_BASE_H_
