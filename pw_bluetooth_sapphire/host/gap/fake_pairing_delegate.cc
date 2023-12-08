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

#include "pw_bluetooth_sapphire/internal/host/gap/fake_pairing_delegate.h"

#include "pw_unit_test/framework.h"

namespace bt::gap {
namespace {

void AddFailure(const char* func_name, PeerId peer_id) {
  return ADD_FAILURE() << "Unexpected call to " << __func__
                       << ", peer_id: " << peer_id.ToString();
}

}  // namespace

FakePairingDelegate::FakePairingDelegate(sm::IOCapability io_capability)
    : io_capability_(io_capability),
      complete_pairing_count_(0),
      confirm_pairing_count_(0),
      display_passkey_count_(0),
      request_passkey_count_(0) {}

FakePairingDelegate::~FakePairingDelegate() {
  if (complete_pairing_cb_ && complete_pairing_count_ == 0) {
    ADD_FAILURE() << "Expected CompletePairing never called";
  }
  if (confirm_pairing_cb_ && confirm_pairing_count_ == 0) {
    ADD_FAILURE() << "Expected ConfirmPairing never called";
  }
  if (display_passkey_cb_ && display_passkey_count_ == 0) {
    ADD_FAILURE() << "Expected DisplayPasskey never called";
  }
  if (request_passkey_cb_ && request_passkey_count_ == 0) {
    ADD_FAILURE() << "Expected RequestPasskey never called";
  }
}

void FakePairingDelegate::CompletePairing(PeerId peer_id, sm::Result<> status) {
  if (!complete_pairing_cb_) {
    AddFailure(__func__, peer_id);
    ADD_FAILURE() << status.error_value();
    return;
  }
  complete_pairing_cb_(peer_id, status);
  complete_pairing_count_++;
}

void FakePairingDelegate::ConfirmPairing(PeerId peer_id,
                                         ConfirmCallback confirm) {
  if (!confirm_pairing_cb_) {
    AddFailure(__func__, peer_id);
    return;
  }
  confirm_pairing_cb_(peer_id, std::move(confirm));
  confirm_pairing_count_++;
}

void FakePairingDelegate::DisplayPasskey(PeerId peer_id,
                                         uint32_t passkey,
                                         DisplayMethod method,
                                         ConfirmCallback confirm) {
  if (!display_passkey_cb_) {
    AddFailure(__func__, peer_id);
    ADD_FAILURE() << "passkey: " << passkey << ", method: " << method;
    return;
  }
  display_passkey_cb_(peer_id, passkey, method, std::move(confirm));
  display_passkey_count_++;
}

void FakePairingDelegate::RequestPasskey(PeerId peer_id,
                                         PasskeyResponseCallback respond) {
  if (!request_passkey_cb_) {
    AddFailure(__func__, peer_id);
    return;
  }
  request_passkey_cb_(peer_id, std::move(respond));
  request_passkey_count_++;
}

}  // namespace bt::gap
