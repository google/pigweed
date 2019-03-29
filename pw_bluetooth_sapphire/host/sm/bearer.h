// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_BEARER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_BEARER_H_

#include <lib/async/cpp/task.h>
#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

#include "src/lib/fxl/macros.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

// Represents the SMP data bearer operating over the fixed SMP L2CAP channel.
// Bearer encapsulates the pairing algorithms described in Vol 3, Part H, 2.3.5
// as distinct sub-procedures that are expected to be driven externally. The
// client is responsible for initiating the right sub-procedure at the right
// time.
//
// A SMP bearer can be instantiated over both LE and BR/EDR transports.
//
// THREAD SAFETY
//
// This class is not thread safe and is meant to be accessed on the thread it
// was created on. All callbacks will be run by the default dispatcher of a
// Bearer's creation thread.
class Bearer final {
 public:
  // Interface for receiving SMP events.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when an on-going pairing procedure terminates with an error.
    // |status| will never indicate success.
    virtual void OnPairingFailed(Status status) = 0;

    // Called when pairing features have been exchanged as a result of the
    // "Pairing Feature Exchange" sub-procedure during Phase 1. This can be
    // called when pairing is either remote or locally initated, with the
    // following parameters:
    //   - |features|: The negotiated features.
    //   - |preq| and |pres|: The SMP "Pairing Request" and "Pairing Response"
    //     command payloads that have been exchanged between the devices. These
    //     values are used to generate "Mconfirm" and "Sconfirm" values used in
    //     LE Legacy Pairing Phase 2 (see Vol 3, Part H, 2.3.5.5). These values
    //     should be ignored if |secure_connections| is true.
    //
    // When the local device is the master, the feature exchange is either
    // initiated directly via InitiateFeatureExchange() or automatically as a
    // response to a "Security Request" received from the slave.
    //
    // When the local device is the slave, the feature exchange is initiated by
    // the master or locally by calling SecurityRequest().
    //
    // TODO(armansito): Support locally initiated "Security Request".
    //
    // The Pairing Feature Exchange procedures will fail if no feature exchange
    // callback is assigned.
    virtual void OnFeatureExchange(const PairingFeatures& features,
                                   const common::ByteBuffer& preq,
                                   const common::ByteBuffer& pres) = 0;

    // Called when a "confirm value" is received from the peer during Legacy
    // Pairing Phase 2.
    virtual void OnPairingConfirm(const common::UInt128& confirm) = 0;

    // Called when a "random value" is received from the peer during Legacy
    // Pairing Phase 2.
    virtual void OnPairingRandom(const common::UInt128& random) = 0;

    // Called when a "Long Term Key" is received from the peer.
    virtual void OnLongTermKey(const common::UInt128& ltk) = 0;

    // Called when EDiv and Rand values are received from the peer during Legacy
    // Pairing Phase 3.
    virtual void OnMasterIdentification(uint16_t ediv, uint64_t random) = 0;

    // Called when the "Identity Resolving Key" is received from the peer during
    // Phase 3.
    virtual void OnIdentityResolvingKey(const common::UInt128& irk) = 0;

    // Called when the "Identity Address" is received from the peer during Phase
    // 3.
    virtual void OnIdentityAddress(const common::DeviceAddress& address) = 0;

    // Called when a "Security Request" is received from the peer (see Vol 3,
    // Part H, 2.4.6) (Note: A device in the link layer master role is not
    // allowed to send this request).
    virtual void OnSecurityRequest(AuthReqField auth_req) = 0;
  };

  // Initializes this Bearer with the following parameters:
  //   - |chan|: The L2CAP SMP fixed channel.
  //   - |role|: The local connection role.
  //   - |secure_connections_supported|: True if the local device supports LE
  //     Secure Connections pairing.
  //   - |io_capability|: The local I/O capability.
  Bearer(fbl::RefPtr<l2cap::Channel> chan, hci::Connection::Role role,
         bool secure_connections_supported, IOCapability io_capability,
         fxl::WeakPtr<Listener> listener);
  ~Bearer() = default;

  // Sets new I/O capabilities. These will be used in future pairing feature
  // exchanges.
  void set_io_capability(IOCapability ioc) { io_capability_ = ioc; }

  // Sets whether or not OOB authentication data is available. False by default.
  void set_oob_available(bool value) { oob_available_ = value; }

  // Sets whether MITM protection is required. False by default.
  void set_mitm_required(bool value) { mitm_required_ = value; }

  // Returns true if pairing has been initiated.
  bool pairing_started() const { return timeout_task_.is_pending(); }

  // Returns the connection role.
  hci::Connection::Role role() const { return role_; }

  // Initiates "Pairing Feature Exchange" with the local device as the
  // initiator (Vol 3, Part H, 2.3). A successful exchange will be indicated by
  // calling via feature exchange callback and failure via the error callback.
  //
  // Returns false if the procedure cannot be initiated because:
  //   - This procedure is already in progress.
  //   - The local device is the slave in the connection.
  //
  // This method can be called on both LE and BR/EDR.
  bool InitiateFeatureExchange();

  // Sends a "confirm value" for Phase 2 of legacy pairing. Returns false if
  // feature exchange is in progress or pairing hasn't been started.
  bool SendConfirmValue(const common::UInt128& confirm);

  // Sends a "random value" for Phase 2 of legacy pairing. Returns false if
  // feature exchange is in progress or pairing hasn't been started.
  bool SendRandomValue(const common::UInt128& random);

  // Sends the encryption information during the key distribution phase
  // (Phase 3) of legacy pairing. Returns false if the command cannot be sent.
  bool SendEncryptionKey(const hci::LinkKey& link_key);

  // Stops the pairing timer. The pairing timer is started when a Pairing
  // Request or Security Request is sent or received and must be explicitly
  // stopped once all required keys have been distributed.
  //
  // A L2CAP link error will be signaled if the timer expires within
  // kPairingTimeout seconds (see smp.h).
  void StopTimer();

  // Ends the current pairing procedure with the given failure |ecode|.
  void Abort(ErrorCode ecode);

 private:
  // Cleans up all pairing state and and invokes the error calback.
  void OnFailure(Status status);

  // Called when the SMP pairing timer expires.
  void OnPairingTimeout();

  // Called to complete a feature exchange. Returns ErrorCode::kNoError if the
  // parameters should be accepted and returns the final values in
  // |out_features|. Returns an error code if the parameters have been rejected
  // and pairing should be aborted.
  ErrorCode ResolveFeatures(bool local_initiator,
                            const PairingRequestParams& preq,
                            const PairingResponseParams& pres,
                            PairingFeatures* out_features);

  // Populates a pairing request/response structure based on features that we
  // support and request. Used to build a SMP PairingRequest and PairingResponse
  // PDU during feature exchange.
  //
  // This does not populate the initiator/responder key distribution and
  // generation fields of |params|. Instead the requested remote and local keys
  // are provided in |out_local_keys| and |out_remote_keys|. The caller is
  // responsible for calculating the appropriate dist/gen values that are
  // suitable for |params| depending on context.
  void BuildPairingParameters(PairingRequestParams* params,
                              KeyDistGenField* out_local_keys,
                              KeyDistGenField* out_remote_keys);

  // Called for SMP commands that are sent by the peer.
  void OnPairingFailed(const PacketReader& reader);
  void OnPairingRequest(const PacketReader& reader);
  void OnPairingResponse(const PacketReader& reader);
  void OnPairingConfirm(const PacketReader& reader);
  void OnPairingRandom(const PacketReader& reader);
  void OnEncryptionInformation(const PacketReader& reader);
  void OnMasterIdentification(const PacketReader& reader);
  void OnIdentityInformation(const PacketReader& reader);
  void OnIdentityAddressInformation(const PacketReader& reader);
  void OnSecurityRequest(const PacketReader& reader);

  // Sends a Pairing Failed command to the peer.
  void SendPairingFailed(ErrorCode ecode);

  // l2cap::Channel callbacks:
  void OnChannelClosed();
  void OnRxBFrame(common::ByteBufferPtr sdu);

  l2cap::ScopedChannel chan_;
  hci::Connection::Role role_;
  bool oob_available_;
  bool mitm_required_;
  bool sc_supported_;
  IOCapability io_capability_;
  fxl::WeakPtr<Listener> listener_;

  uint8_t mtu_;

  // We use this buffer to store pairing request and response PDUs as they are
  // needed to complete the feature exchange (i.e. the "preq" and "pres"
  // payloads needed for Phase 2 (see Vol 3, Part H, 2.2.3 for example).
  common::StaticByteBuffer<sizeof(Header) + sizeof(PairingRequestParams)>
      pairing_payload_buffer_;

  // Task used to drive the "SMP Timeout" (Vol 3, Part H, 3.4). The timer is
  // started when pairing is initiated.
  async::TaskClosureMethod<Bearer, &Bearer::OnPairingTimeout> timeout_task_{
      this};

  // True if a pairing feature exchange has been initiated and waiting for a
  // response.
  bool feature_exchange_pending_;

  fxl::WeakPtrFactory<Bearer> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Bearer);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_BEARER_H_
