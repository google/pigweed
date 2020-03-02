// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_FAKE_PHASE_LISTENER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_FAKE_PHASE_LISTENER_H_

#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

class FakeListener : public PairingPhase::Listener {
 public:
  FakeListener() : weak_ptr_factory_(this) {}
  ~FakeListener() override = default;

  // PairingPhase::Listener override:
  bool StartTimer() override {
    timer_started_count_++;
    return is_timer_working_;
  };

  // PairingPhase::Listener override:
  std::optional<IdentityInfo> OnIdentityRequest() override {
    identity_info_count_++;
    return identity_info_;
  }

  // PairingPhase::Listener override:
  void OnTemporaryKeyRequest(PairingMethod method, TkResponse responder) override {
    if (tk_delegate_) {
      tk_delegate_(method, std::move(responder));
    } else {
      responder(true /* success */, 0);
    }
  }

  // PairingPhase::Listener override:
  void OnNewLongTermKey(const LTK& ltk) override {
    ltk_count_++;
    ltk_ = ltk;
  }

  // PairingPhase::Listener override:
  void OnPairingFailed(Status error) override {
    pairing_error_count_++;
    last_error_ = error;
  }

  fxl::WeakPtr<PairingPhase::Listener> as_weak_ptr() { return weak_ptr_factory_.GetWeakPtr(); }

  void set_timer_working(bool timer_working) { is_timer_working_ = timer_working; }
  int timer_started_count() const { return timer_started_count_; }

  void set_identity_info(std::optional<IdentityInfo> value) { identity_info_ = value; }
  int identity_info_count() const { return identity_info_count_; }

  using TkDelegate = fit::function<void(PairingMethod, TkResponse)>;
  void set_tk_delegate(TkDelegate delegate) { tk_delegate_ = std::move(delegate); }

  int pairing_error_count() const { return pairing_error_count_; }
  Status last_error() const { return last_error_; }

  int ltk_count() const { return ltk_count_; }
  const LTK& ltk() const { return ltk_; }

 private:
  bool is_timer_working_ = true;
  int timer_started_count_ = 0;

  std::optional<IdentityInfo> identity_info_ = std::nullopt;
  int identity_info_count_ = 0;

  // Callback used to notify when a call to OnTKRequest() is received.
  // OnTKRequest() will reply with 0 if a callback is not set.
  TkDelegate tk_delegate_;

  int ltk_count_ = 0;
  LTK ltk_;

  int pairing_error_count_ = 0;
  Status last_error_;

  fxl::WeakPtrFactory<FakeListener> weak_ptr_factory_;
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_FAKE_PHASE_LISTENER_H_
