// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_1_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_1_H_

#include <lib/async/cpp/task.h>
#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/active_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {
// Responsible for Phase 1 of SMP pairing, the feature exchange. Takes in the current pairing state
// and notifies a constructor-provided callback with the negotiated features upon completion.
//
// This class is not thread safe and is meant to be accessed on the thread it was created on. All
// callbacks will be run by the default dispatcher of a Phase1's creation thread.
class Phase1 final : public ActivePhase, public PairingChannelHandler {
 public:
  // Called when Phase 1 completes successfully. |features| are the negotiated features. |preq| and
  // |pres| are the SMP "Pairing Request" and "Pairing Response" payloads exchanged by the devices,
  // which are returned for use in Phase 2 crypto calculations.
  using CompleteCallback = fit::function<void(PairingFeatures features, PairingRequestParams preq,
                                              PairingResponseParams pres)>;

  // Returns a Phase 1 in the initiator role. Note the lack of a `preq` parameter: Phase 1 builds &
  // sends the Pairing Request as initiator. See private ctor for parameter descriptions.
  static std::unique_ptr<Phase1> CreatePhase1Initiator(fxl::WeakPtr<PairingChannel> chan,
                                                       fxl::WeakPtr<Listener> listener,
                                                       IOCapability io_capability,
                                                       BondableMode bondable_mode,
                                                       bool mitm_required,
                                                       CompleteCallback on_complete);

  // Returns a Phase 1 in the responder role. Note the `preq` parameter: Phase 1 is supplied the
  // Pairing Request from the remote as responder. See private ctor for parameter descriptions.
  static std::unique_ptr<Phase1> CreatePhase1Responder(
      fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, PairingRequestParams preq,
      IOCapability io_capability, BondableMode bondable_mode, bool mitm_required,
      CompleteCallback on_complete);

  ~Phase1() override = default;

  // Runs the Phase 1 state machine, performing the feature exchange.
  void Start() final;

 private:
  //   |chan|, |listener|, and |role|: used to construct the base ActivePhase
  //   |preq|: If empty, the device is in the initiator role and starts the pairing.
  //           If present, the device is in the responder role, and will respond to |preq|, the
  //           peer initiator's pairing request.
  //   |bondable_mode|: the bondable mode of the local device (see V5.1 Vol. 3 Part C Section 9.4).
  //   |on_complete|: called at the end of Phase 1 with the resulting features.
  Phase1(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
         std::optional<PairingRequestParams> preq, IOCapability io_capability,
         BondableMode bondable_mode, bool mitm_required, CompleteCallback on_complete);

  // Start the feature exchange by sending the Pairing Request. The local device must be in the SMP
  // initiator role (V5.1 Vol 3, Part H, 2.3).
  void InitiateFeatureExchange();

  // Handle the peer-initiated feature exchange. The local device must be in the SMP responder role
  // (V5.1 Vol 3, Part H, 2.3).
  void RespondToPairingRequest(const PairingRequestParams& req_params);

  // The returned `LocalPairingParams` structure contains the locally-preferred pairing parameters.
  // The caller is responsible for making any adjustments necessary (e.g. due to peer preferences)
  // before converting the `LocalPairingParams` to SMP PairingResponse/PairingRequest PDUs.
  LocalPairingParams BuildPairingParameters();

  // Called to complete a feature exchange. Returns the resulting PairingFeatures if the parameters
  // should be accepted, or an error code if the parameters are rejected and pairing should abort.
  fit::result<PairingFeatures, ErrorCode> ResolveFeatures(bool local_initiator,
                                                          const PairingRequestParams& preq,
                                                          const PairingResponseParams& pres);

  // Called for SMP commands sent by the peer.
  void OnPairingResponse(const PairingResponseParams& response_params);

  // PairingChannelHandler callbacks:
  void OnRxBFrame(ByteBufferPtr sdu) final;
  void OnChannelClosed() final { ActivePhase::HandleChannelClosed(); }

  // PairingPhase override
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() final {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // If acting as the Responder to a peer-initiatied pairing, then |preq_| is the |preq| ctor
  // parameter. Otherwise, these are generated and exchanged during Phase 1.
  std::optional<PairingRequestParams> preq_;
  std::optional<PairingResponseParams> pres_;
  // Local feature flags that determine the feature exchange negotiation with the peer.
  bool oob_available_;
  bool mitm_required_;
  bool feature_exchange_pending_;
  IOCapability io_capability_;
  BondableMode bondable_mode_;

  CompleteCallback on_complete_;

  fxl::WeakPtrFactory<Phase1> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Phase1);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_1_H_
