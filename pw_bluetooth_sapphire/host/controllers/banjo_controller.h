// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_BANJO_CONTROLLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_BANJO_CONTROLLER_H_

#include <fuchsia/hardware/bt/hci/cpp/banjo.h>
#include <fuchsia/hardware/bt/vendor/cpp/banjo.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/channel.h>

#include <mutex>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "pw_bluetooth/controller.h"
#include "pw_bluetooth/vendor.h"

namespace bt::controllers {

// BanjoController is an implementation of Controller that uses the fuchsia.hardware.bt.hci/BtHci
// and fuchsia.hardware.bt.vendor/BtVendor Banjo protocols to communicate with transport drivers.
class BanjoController final : public pw::bluetooth::Controller {
 public:
  using PwStatusCallback = pw::Callback<void(pw::Status)>;

  // |vendor_proto| is optional. If the transport driver does not support the BtVendor protocol,
  // this may be nullopt.
  // |dispatcher| must outlive this object.
  BanjoController(ddk::BtHciProtocolClient hci_proto,
                  std::optional<ddk::BtVendorProtocolClient> vendor_proto,
                  async_dispatcher_t* dispatcher);

  ~BanjoController() override;

  // Controller overrides:
  void SetEventFunction(DataFunction func) override { event_cb_ = std::move(func); }

  void SetReceiveAclFunction(DataFunction func) override { acl_cb_ = std::move(func); }

  void SetReceiveScoFunction(DataFunction func) override { sco_cb_ = std::move(func); }

  void Initialize(PwStatusCallback complete_callback, PwStatusCallback error_callback) override;

  void Close(PwStatusCallback callback) override;

  void SendCommand(pw::span<const std::byte> command) override;

  void SendAclData(pw::span<const std::byte> data) override;

  void SendScoData(pw::span<const std::byte> data) override;

  void ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding, ScoSampleRate sample_rate,
                    pw::Callback<void(pw::Status)> callback) override;

  void ResetSco(pw::Callback<void(pw::Status)> callback) override;

  void GetFeatures(pw::Callback<void(FeaturesBits)> callback) override;

  void EncodeVendorCommand(
      pw::bluetooth::VendorCommandParameters parameters,
      pw::Callback<void(pw::Result<pw::span<const std::byte>>)> callback) override;

 private:
  using ZxStatusCallback = fit::callback<void(zx_status_t)>;

  // Used by Banjo callbacks to detect stack destruction & to dispatch callbacks onto the bt-host
  // thread.
  struct CallbackData : public fbl::RefCounted<CallbackData> {
    // Lock to guard reads/writes to the |dispatcher| pointer variable below (not the underlying
    // dispatcher). Calls to async::PostTask and async::WaitBase::Begin should be considered reads,
    // and require the lock to be held.
    std::mutex lock;
    // Set to nullptr on BanjoController destruction to indicate to Banjo callbacks, which may run
    // on an HCI driver thread, that they should do nothing. It is safe to access |dispatcher| on a
    // different thread than |BanjoController::dispatcher_| because operations on the underying
    // dispatcher, including waiting for signals and posting tasks, are thread-safe. The only
    // concern is that the callbacks would use the dispatcher after it is destroyed and this pointer
    // is invalid, but that is impossible because the dispatcher outlives BanjoController, and
    // BanjoController sets |dispatcher| to null upon destruction.
    async_dispatcher_t* dispatcher __TA_GUARDED(lock);
  };

  // Call to report an error to the client.
  void OnError(zx_status_t status);

  void CleanUp();

  // Wraps a callback in a callback that posts the callback to the bt-host thread.
  PwStatusCallback ThreadSafeCallbackWrapper(PwStatusCallback callback);

  void InitializeWait(async::WaitBase& wait, zx::channel& channel);

  void OnChannelSignal(const char* chan_name, zx_status_t status, async::WaitBase* wait,
                       const zx_packet_signal_t* signal, pw::span<std::byte> buffer,
                       zx::channel& channel, DataFunction& data_cb);

  void OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal);

  void OnCommandSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                       const zx_packet_signal_t* signal);

  void OnScoSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal);

  pw::bluetooth::Controller::FeaturesBits BanjoVendorFeaturesToFeaturesBits(
      bt_vendor_features_t features);

  ddk::BtHciProtocolClient hci_proto_;
  std::optional<ddk::BtVendorProtocolClient> vendor_proto_;

  zx::channel acl_channel_;
  zx::channel command_channel_;
  zx::channel sco_channel_;

  DataFunction event_cb_;
  DataFunction acl_cb_;
  DataFunction sco_cb_;
  PwStatusCallback error_cb_;

  async::WaitMethod<BanjoController, &BanjoController::OnAclSignal> acl_wait_{this};
  async::WaitMethod<BanjoController, &BanjoController::OnCommandSignal> command_wait_{this};
  async::WaitMethod<BanjoController, &BanjoController::OnScoSignal> sco_wait_{this};

  async_dispatcher_t* dispatcher_;

  fbl::RefPtr<CallbackData> callback_data_;
};

}  // namespace bt::controllers

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_BANJO_CONTROLLER_H_
