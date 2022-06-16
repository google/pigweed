// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sc_stage_1_just_works_numeric_comparison.h"

#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::sm {

ScStage1JustWorksNumericComparison::ScStage1JustWorksNumericComparison(
    fxl::WeakPtr<PairingPhase::Listener> listener, Role role, UInt256 local_pub_key_x,
    UInt256 peer_pub_key_x, PairingMethod method, fxl::WeakPtr<PairingChannel> sm_chan,
    Stage1CompleteCallback on_complete)
    : listener_(std::move(listener)),
      role_(role),
      local_public_key_x_(local_pub_key_x),
      peer_public_key_x_(peer_pub_key_x),
      method_(method),
      sent_pairing_confirm_(false),
      local_rand_(Random<UInt128>()),
      sent_local_rand_(false),
      peer_rand_(),
      sm_chan_(std::move(sm_chan)),
      on_complete_(std::move(on_complete)),
      weak_ptr_factory_(this) {
  ZX_ASSERT(method == PairingMethod::kJustWorks || method == PairingMethod::kNumericComparison);
}

void ScStage1JustWorksNumericComparison::Run() {
  // The responder sends the Pairing Confirm message to start Stage 1, so as initiator there is
  // nothing to do besides wait for the peer's Confirm value (Vol 3, Part H, 2.3.5.6.2).
  if (role_ == Role::kResponder) {
    std::optional<UInt128> maybe_confirm =
        util::F4(local_public_key_x_, peer_public_key_x_, local_rand_, 0);
    if (!maybe_confirm.has_value()) {
      bt_log(WARN, "sm", "unable to calculate confirm value in SC Phase 1");
      on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
      return;
    }
    responder_confirm_ = *maybe_confirm;
    sm_chan_->SendMessage(kPairingConfirm, *responder_confirm_);
    sent_pairing_confirm_ = true;
  }
}

void ScStage1JustWorksNumericComparison::OnPairingConfirm(PairingConfirmValue confirm) {
  // Only the responder can send the confirm value to the initiator (Vol 3, Part H, 2.3.5.6.2).
  if (role_ == Role::kResponder) {
    bt_log(WARN, "sm",
           "cannot accept pairing confirm in SC Numeric Comparison/Just Works responder mode");
    on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
    return;
  }
  if (responder_confirm_.has_value()) {
    bt_log(WARN, "sm",
           "received multiple Pairing Confirm values in SC Numeric Comparison/Just Works");
    on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
    return;
  }
  responder_confirm_ = confirm;
  // We already know that we're the initiator at this point, which sends the Random immediately
  // after receiving the Confirm.
  SendPairingRandom();
}

void ScStage1JustWorksNumericComparison::SendPairingRandom() {
  // The random value is always sent after the confirm exchange (Vol 3, Part H, 2.3.5.6.2).
  ZX_ASSERT(responder_confirm_.has_value());
  ZX_ASSERT(!sent_local_rand_);
  if (role_ == Role::kResponder) {
    ZX_ASSERT(peer_rand_.has_value());
  }
  sm_chan_->SendMessage(kPairingRandom, local_rand_);
  sent_local_rand_ = true;
  if (role_ == Role::kResponder) {
    CompleteStage1();
  }
}

void ScStage1JustWorksNumericComparison::OnPairingRandom(PairingRandomValue rand) {
  if (!responder_confirm_.has_value()) {
    bt_log(WARN, "sm", "received Pairing Random before the confirm value was exchanged");
    on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
    return;
  }
  if (peer_rand_.has_value()) {
    bt_log(WARN, "sm", "received multiple Pairing Random values from peer");
    on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
    return;
  }
  peer_rand_ = rand;
  if (role_ == Role::kResponder) {
    SendPairingRandom();
    return;
  }
  // Otherwise, we're the initiator & we must validate the |responder_confirm_| with |rand|.
  if (!sent_local_rand_) {
    bt_log(WARN, "sm", "received peer random before sending our own as initiator");
    on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
    return;
  }
  std::optional<UInt128> maybe_confirm_check =
      util::F4(peer_public_key_x_, local_public_key_x_, rand, 0);
  if (!maybe_confirm_check.has_value()) {
    bt_log(WARN, "sm", "unable to calculate SC confirm check value");
    on_complete_(fitx::error(ErrorCode::kConfirmValueFailed));
    return;
  }
  if (*maybe_confirm_check != *responder_confirm_) {
    bt_log(WARN, "sm", "peer SC confirm value did not match check, aborting");
    on_complete_(fitx::error(ErrorCode::kConfirmValueFailed));
    return;
  }
  CompleteStage1();
}

void ScStage1JustWorksNumericComparison::CompleteStage1() {
  ZX_ASSERT(responder_confirm_.has_value());
  ZX_ASSERT(peer_rand_.has_value());
  ZX_ASSERT(sent_local_rand_);
  const auto& [initiator_rand, responder_rand] = util::MapToRoles(local_rand_, *peer_rand_, role_);
  const auto& [initiator_pub_key_x, responder_pub_key_x] =
      util::MapToRoles(local_public_key_x_, peer_public_key_x_, role_);
  auto results = Output{.initiator_r = {0},
                        .responder_r = {0},
                        .initiator_rand = initiator_rand,
                        .responder_rand = responder_rand};
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (method_ == PairingMethod::kNumericComparison) {
    std::optional<uint32_t> g2_result =
        util::G2(initiator_pub_key_x, responder_pub_key_x, initiator_rand, responder_rand);
    if (!g2_result.has_value()) {
      bt_log(WARN, "sm", "unable to calculate numeric comparison user check");
      on_complete_(fitx::error(ErrorCode::kNumericComparisonFailed));
      return;
    }

    // The code displayed to the user is the least significant 6 digits of the G2 function.
    uint32_t comparison_code = *g2_result % 1000000;
    listener_->DisplayPasskey(
        comparison_code, Delegate::DisplayMethod::kComparison,
        [self, results](bool passkey_confirmed) {
          bt_log(INFO, "sm", "PairingDelegate %s SC numeric display pairing",
                 passkey_confirmed ? "accepted" : "rejected");
          if (self) {
            passkey_confirmed
                ? self->on_complete_(fitx::ok(results))
                : self->on_complete_(fitx::error(ErrorCode::kNumericComparisonFailed));
          }
        });
  } else {  // method == kJustWorks
    listener_->ConfirmPairing([self, results](bool user_confirmed) {
      bt_log(INFO, "sm", "PairingDelegate %s SC just works pairing",
             user_confirmed ? "accepted" : "rejected");
      if (self) {
        user_confirmed ? self->on_complete_(fitx::ok(results))
                       : self->on_complete_(fitx::error(ErrorCode::kUnspecifiedReason));
      }
    });
  }
}

}  // namespace bt::sm
