// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_FIDL_CONTROLLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_FIDL_CONTROLLER_H_

#include <fuchsia/hardware/bluetooth/cpp/fidl.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/channel.h>

#include "pw_bluetooth/controller.h"

namespace bt::controllers {

class FidlController final : public pw::bluetooth::Controller {
 public:
  using PwStatusCallback = pw::Callback<void(pw::Status)>;

  // |dispatcher| must outlive this object.
  FidlController(fuchsia::hardware::bluetooth::HciHandle hci, async_dispatcher_t* dispatcher);

  ~FidlController() override;

  // Controller overrides:
  void SetEventFunction(DataFunction func) override { event_cb_ = std::move(func); }

  void SetReceiveAclFunction(DataFunction func) override { acl_cb_ = std::move(func); }

  void Initialize(PwStatusCallback complete_callback, PwStatusCallback error_callback) override;

  void Close(PwStatusCallback callback) override;

  void SendCommand(pw::span<const std::byte> command) override;

  void SendAclData(pw::span<const std::byte> data) override;

  void SetReceiveScoFunction(DataFunction func) override {}
  void SendScoData(pw::span<const std::byte> data) override {}
  void ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding, ScoSampleRate sample_rate,
                    pw::Callback<void(pw::Status)> callback) override {}
  void ResetSco(pw::Callback<void(pw::Status)> callback) override {}
  void GetFeatures(pw::Callback<void(FeaturesBits)> callback) override {}
  void EncodeVendorCommand(
      pw::bluetooth::VendorCommandParameters parameters,
      pw::Callback<void(pw::Result<pw::span<const std::byte>>)> callback) override {}

 private:
  void OnError(zx_status_t status);

  void CleanUp();

  void InitializeWait(async::WaitBase& wait, zx::channel& channel);

  void OnChannelSignal(const char* chan_name, zx_status_t status, async::WaitBase* wait,
                       const zx_packet_signal_t* signal, pw::span<std::byte> buffer,
                       zx::channel& channel, DataFunction& data_cb);

  void OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal);

  void OnCommandSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                       const zx_packet_signal_t* signal);

  // hci_handle_ holds the Hci channel until Initialize() is called, at which point hci_ is bound to
  // the channel. This prevents errors from being lost before initialization.
  fuchsia::hardware::bluetooth::HciHandle hci_handle_;
  fuchsia::hardware::bluetooth::HciPtr hci_;

  async_dispatcher_t* dispatcher_;

  zx::channel acl_channel_;
  zx::channel command_channel_;

  DataFunction event_cb_;
  DataFunction acl_cb_;
  PwStatusCallback error_cb_;

  async::WaitMethod<FidlController, &FidlController::OnAclSignal> acl_wait_{this};
  async::WaitMethod<FidlController, &FidlController::OnCommandSignal> command_wait_{this};
};

}  // namespace bt::controllers

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_FIDL_CONTROLLER_H_
