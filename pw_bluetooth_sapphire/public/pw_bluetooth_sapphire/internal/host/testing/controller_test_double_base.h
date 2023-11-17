// Copyright 2023 The Pigweed Authors
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
#include <pw_async/dispatcher.h>
#include <pw_async/heap_dispatcher.h>

#include "pw_bluetooth/controller.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"

namespace bt::testing {

// Abstract base for implementing a fake HCI controller endpoint. This can
// directly send ACL data and event packets on request and forward outgoing ACL
// data packets to subclass implementations.
class ControllerTestDoubleBase : public pw::bluetooth::Controller {
 public:
  using PwStatusCallback = pw::Callback<void(pw::Status)>;

  using EncodeVendorCommandFunction = fit::function<void(
      pw::bluetooth::VendorCommandParameters,
      pw::Callback<void(pw::Result<pw::span<const std::byte>>)>)>;

  using ConfigureScoFunction =
      fit::function<void(ScoCodingFormat,
                         ScoEncoding,
                         ScoSampleRate,
                         fit::callback<void(pw::Status)>)>;

  using ResetScoFunction = fit::function<void(fit::callback<void(pw::Status)>)>;

  explicit ControllerTestDoubleBase(pw::async::Dispatcher& pw_dispatcher);
  ~ControllerTestDoubleBase() override;

  pw::async::Dispatcher& pw_dispatcher() { return pw_dispatcher_; }
  pw::async::HeapDispatcher& heap_dispatcher() { return heap_dispatcher_; }

  // Sends the given packet over this FakeController's command channel endpoint.
  // Returns the result of the write operation on the command channel.
  bool SendCommandChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this FakeController's ACL data channel
  // endpoint.
  // Returns the result of the write operation on the channel.
  bool SendACLDataChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this ControllerTestDouble's SCO data channel
  // endpoint.
  // Returns the result of the write operation on the channel.
  bool SendScoDataChannelPacket(const ByteBuffer& packet);

  // Wrapper around SignalError() to support old test code.
  void Stop() { SignalError(pw::Status::Aborted()); }

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

  void set_configure_sco_cb(ConfigureScoFunction cb) {
    configure_sco_cb_ = std::move(cb);
  }

  void set_reset_sco_cb(ResetScoFunction cb) { reset_sco_cb_ = std::move(cb); }

  // Controller overrides:
  void SetEventFunction(DataFunction func) override {
    event_cb_ = std::move(func);
  }

  void SetReceiveAclFunction(DataFunction func) override {
    acl_cb_ = std::move(func);
  }

  void SetReceiveScoFunction(DataFunction func) override {
    sco_cb_ = std::move(func);
  }

  void Initialize(PwStatusCallback complete_callback,
                  PwStatusCallback error_callback) override;

  void Close(PwStatusCallback callback) override;

  void ConfigureSco(ScoCodingFormat coding_format,
                    ScoEncoding encoding,
                    ScoSampleRate sample_rate,
                    pw::Callback<void(pw::Status)> callback) override;

  void ResetSco(pw::Callback<void(pw::Status)> callback) override;

  void GetFeatures(pw::Callback<void(FeaturesBits)> callback) override {
    callback(features_);
  }

  void EncodeVendorCommand(
      pw::bluetooth::VendorCommandParameters parameters,
      pw::Callback<void(pw::Result<pw::span<const std::byte>>)> callback)
      override {
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

  pw::async::Dispatcher& pw_dispatcher_;
  pw::async::HeapDispatcher heap_dispatcher_;
  WeakSelf<ControllerTestDoubleBase> weak_self_{this};
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ControllerTestDoubleBase);
};

}  // namespace bt::testing
