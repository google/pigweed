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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_FAKE_PAIRING_DELEGATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_FAKE_PAIRING_DELEGATE_H_

#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/gap/pairing_delegate.h"

namespace bt::gap {

// Adapts PairingDelegate to generic callbacks that can perform any desired test
// checking. If an PairingDelegate call is made that does not have a
// corresponding callback set, a Google Test failure is added. If this object is
// destroyed and there are callback-assigned PairingDelegate calls that were not
// called, a Google Test failure is added.
class FakePairingDelegate final : public PairingDelegate {
 public:
  explicit FakePairingDelegate(sm::IOCapability io_capability);
  ~FakePairingDelegate() override;

  void set_io_capability(sm::IOCapability io_capability) {
    io_capability_ = io_capability;
  }

  // If set, these will receive calls to their respective calls. If not set,
  // the corresponding PairingDelegate call will result in a test failure.
  using CompletePairingCallback = fit::function<void(PeerId, sm::Result<>)>;
  void SetCompletePairingCallback(CompletePairingCallback cb) {
    complete_pairing_cb_ = std::move(cb);
  }
  using ConfirmPairingCallback = fit::function<void(PeerId, ConfirmCallback)>;
  void SetConfirmPairingCallback(ConfirmPairingCallback cb) {
    confirm_pairing_cb_ = std::move(cb);
  }
  using DisplayPasskeyCallback =
      fit::function<void(PeerId, uint32_t, DisplayMethod, ConfirmCallback)>;
  void SetDisplayPasskeyCallback(DisplayPasskeyCallback cb) {
    display_passkey_cb_ = std::move(cb);
  }
  using RequestPasskeyCallback =
      fit::function<void(PeerId, PasskeyResponseCallback)>;
  void SetRequestPasskeyCallback(RequestPasskeyCallback cb) {
    request_passkey_cb_ = std::move(cb);
  }

  // PairingDelegate overrides.
  sm::IOCapability io_capability() const override { return io_capability_; }
  void CompletePairing(PeerId peer_id, sm::Result<> status) override;
  void ConfirmPairing(PeerId peer_id, ConfirmCallback confirm) override;
  void DisplayPasskey(PeerId peer_id,
                      uint32_t passkey,
                      DisplayMethod method,
                      ConfirmCallback confirm) override;
  void RequestPasskey(PeerId peer_id, PasskeyResponseCallback respond) override;

 private:
  sm::IOCapability io_capability_;
  CompletePairingCallback complete_pairing_cb_;
  ConfirmPairingCallback confirm_pairing_cb_;
  DisplayPasskeyCallback display_passkey_cb_;
  RequestPasskeyCallback request_passkey_cb_;
  int complete_pairing_count_;
  int confirm_pairing_count_;
  int display_passkey_count_;
  int request_passkey_count_;
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_FAKE_PAIRING_DELEGATE_H_
