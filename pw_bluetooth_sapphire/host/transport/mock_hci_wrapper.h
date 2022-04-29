// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_HCI_WRAPPER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_HCI_WRAPPER_H_

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

#include "hci_wrapper.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::hci::testing {

// MockHciWrapper provides a mockable implementation of HciWrapper. It enables packets to be sent
// directly between test fixtures and test controllers without the use of channels or Banjo APIs.
class MockHciWrapper : public HciWrapper {
 public:
  using SendAclPacketFunction = fit::function<zx_status_t(std::unique_ptr<ACLDataPacket>)>;

  using SendCommandPacketFunction = fit::function<zx_status_t(std::unique_ptr<CommandPacket>)>;

  using SendScoPacketFunction = fit::function<zx_status_t(std::unique_ptr<ScoDataPacket>)>;

  using ResetScoFunction = fit::function<void(StatusCallback)>;

  using ConfigureScoFunction =
      fit::function<void(ScoCodingFormat, ScoEncoding, ScoSampleRate, StatusCallback)>;

  using EncodeAclPriorityCommandFunction =
      fit::function<fitx::result<zx_status_t, DynamicByteBuffer>(hci_spec::ConnectionHandle,
                                                                 hci::AclPriority)>;

  void set_send_acl_cb(SendAclPacketFunction cb) { send_acl_cb_ = std::move(cb); }

  void set_send_command_cb(SendCommandPacketFunction cb) { send_command_cb_ = std::move(cb); }

  void set_send_sco_cb(SendScoPacketFunction cb) { send_sco_cb_ = std::move(cb); }

  void set_sco_supported(bool supported) { sco_supported_ = supported; }

  void SimulateError(zx_status_t status) {
    if (error_cb_) {
      error_cb_(status);
    }
  }
  void ReceiveEvent(std::unique_ptr<EventPacket> packet) {
    // Post packet to simulate async channel signal behavior that many tests expect.
    async::PostTask(dispatcher_, [this, packet = std::move(packet)]() mutable {
      if (event_packet_cb_) {
        event_packet_cb_(std::move(packet));
      }
    });
  }

  void ReceiveAclPacket(std::unique_ptr<ACLDataPacket> packet) {
    async::PostTask(dispatcher_, [this, packet = std::move(packet)]() mutable {
      if (acl_packet_cb_) {
        acl_packet_cb_(std::move(packet));
      }
    });
  }

  void ReceiveScoPacket(std::unique_ptr<ScoDataPacket> packet) {
    async::PostTask(dispatcher_, [this, packet = std::move(packet)]() mutable {
      if (sco_packet_cb_) {
        sco_packet_cb_(std::move(packet));
      }
    });
  }

  void SetResetScoCallback(ResetScoFunction callback) { reset_sco_cb_ = std::move(callback); }

  void set_configure_sco_callback(ConfigureScoFunction callback) {
    configure_sco_cb_ = std::move(callback);
  }

  void SetVendorFeatures(VendorFeaturesBits features) { vendor_features_ = features; }

  void SetEncodeAclPriorityCommandCallback(EncodeAclPriorityCommandFunction callback) {
    encode_acl_priority_command_cb_ = std::move(callback);
  }

  fxl::WeakPtr<MockHciWrapper> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // HciWrapper overrides:
  bool Initialize(ErrorCallback error_callback) override {
    initialized_ = true;
    error_cb_ = std::move(error_callback);
    return true;
  }

  zx_status_t SendCommand(std::unique_ptr<CommandPacket> packet) override {
    return send_command_cb_ ? send_command_cb_(std::move(packet)) : ZX_OK;
  }

  void SetEventCallback(EventPacketFunction callback) override {
    event_packet_cb_ = std::move(callback);
  }

  zx_status_t SendAclPacket(std::unique_ptr<ACLDataPacket> packet) override {
    return send_acl_cb_ ? send_acl_cb_(std::move(packet)) : ZX_OK;
  }

  void SetAclCallback(AclPacketFunction callback) override { acl_packet_cb_ = std::move(callback); }

  zx_status_t SendScoPacket(std::unique_ptr<ScoDataPacket> packet) override {
    return send_sco_cb_ ? send_sco_cb_(std::move(packet)) : ZX_OK;
  }

  void SetScoCallback(ScoPacketFunction callback) override { sco_packet_cb_ = std::move(callback); }

  bool IsScoSupported() override { return sco_supported_; }

  void ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding, ScoSampleRate sample_rate,
                    StatusCallback callback) override {
    // Post the callback to simulate post in HciWrapper.
    StatusCallback callback_wrapper = [=, cb = std::move(callback),
                                       dispatcher = dispatcher_](zx_status_t status) mutable {
      async::PostTask(dispatcher, [status, cb = std::move(cb)]() mutable { cb(status); });
    };
    configure_sco_cb_
        ? configure_sco_cb_(coding_format, encoding, sample_rate, std::move(callback_wrapper))
        : callback_wrapper(ZX_OK);
  }

  void ResetSco(StatusCallback callback) override {
    StatusCallback callback_wrapper = [=, cb = std::move(callback),
                                       dispatcher = dispatcher_](zx_status_t status) mutable {
      async::PostTask(dispatcher, [status, cb = std::move(cb)]() mutable { cb(status); });
    };
    reset_sco_cb_ ? reset_sco_cb_(std::move(callback_wrapper))
                  : callback_wrapper(ZX_ERR_NOT_SUPPORTED);
  }

  VendorFeaturesBits GetVendorFeatures() override { return vendor_features_; }

  fitx::result<zx_status_t, DynamicByteBuffer> EncodeSetAclPriorityCommand(
      hci_spec::ConnectionHandle connection, hci::AclPriority priority) override {
    return encode_acl_priority_command_cb_ ? encode_acl_priority_command_cb_(connection, priority)
                                           : fitx::error(ZX_ERR_NOT_SUPPORTED);
  }

 private:
  bool initialized_ = false;
  bool sco_supported_ = false;
  VendorFeaturesBits vendor_features_ = static_cast<VendorFeaturesBits>(0);

  ErrorCallback error_cb_;

  SendScoPacketFunction send_sco_cb_;
  SendCommandPacketFunction send_command_cb_;
  SendAclPacketFunction send_acl_cb_;

  EventPacketFunction event_packet_cb_;
  AclPacketFunction acl_packet_cb_;
  ScoPacketFunction sco_packet_cb_;

  ResetScoFunction reset_sco_cb_;
  ConfigureScoFunction configure_sco_cb_;
  EncodeAclPriorityCommandFunction encode_acl_priority_command_cb_;

  async_dispatcher_t* dispatcher_{async_get_default_dispatcher()};

  fxl::WeakPtrFactory<MockHciWrapper> weak_ptr_factory_{this};
};

}  // namespace bt::hci::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_HCI_WRAPPER_H_
