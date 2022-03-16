// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SECURITY_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SECURITY_MANAGER_H_

#include <zircon/assert.h>

#include <memory>
#include <queue>
#include <variant>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/error.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_1.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_2_legacy.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_2_secure_connections.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_3.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_request_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::sm {

// SecurityManager provides a per-peer interface to Security Manager Protocol functionality in v5.2
// Vol. 3 Part H. The peer device must be a LE or BR/EDR/LE device. SecurityManager is an abstract
// class so that SM-dependent code can dependency-inject test doubles in unit tests.
//
// The production implementation of SecurityManager is the SecurityManagerImpl class, which
// implements the functionality detailed in README.md. Clients should obtain a production object
// through the SecurityManager::Create factory function.
//
// A SecurityManager test double can be obtained through `TestSecurityManager::Create`.
//
/// See README.md for more overview of this library.
class SecurityManager {
 public:
  // Factory function which returns a production SecurityManager instance:
  // |link|: The LE logical link over which pairing procedures occur.
  // |smp|: The L2CAP LE SMP fixed channel that operates over |link|.
  // |io_capability|: The initial I/O capability.
  // |delegate|: Delegate which handles SMP interactions with the rest of the Bluetooth stack.
  // |bondable_mode|: the operating bondable mode of the device (see v5.2, Vol. 3, Part C 9.4).
  // |security_mode|: the security mode of this SecurityManager (see v5.2, Vol. 3, Part C 10.2).
  static std::unique_ptr<SecurityManager> Create(fxl::WeakPtr<hci::Connection> link,
                                                 fbl::RefPtr<l2cap::Channel> smp,
                                                 IOCapability io_capability,
                                                 fxl::WeakPtr<Delegate> delegate,
                                                 BondableMode bondable_mode,
                                                 gap::LESecurityMode security_mode);
  virtual ~SecurityManager() = default;
  // Assigns the requested |ltk| to this connection, adopting the security properties of |ltk|. If
  // the local device is the central of the underlying link, then the link layer authentication
  // procedure will be initiated.
  //
  // Returns false if a pairing procedure is in progress when this method is called. If the link
  // layer authentication procedure fails, then the link will be disconnected by the controller
  // (Vol 2, Part E, 7.8.24; hci::Connection guarantees this by severing the link directly).
  //
  // This function is mainly intended to assign an existing LTK to a connection (e.g. from bonding
  // data). This function overwrites any previously assigned LTK.
  virtual bool AssignLongTermKey(const LTK& ltk) = 0;

  // TODO(fxbug.dev/52937): Add function to register a BR/EDR link and SMP channel.

  // Attempt to raise the security level of the connection to the desired |level| and notify the
  // result in |callback|.
  //
  // If the desired security properties are already satisfied, this procedure will succeed
  // immediately (|callback| will be run with the current security properties).
  //
  // If a pairing procedure has already been initiated (either by us or the peer), the request will
  // be queued and |callback| will be notified when the procedure completes. If the resulting
  // security level does not satisfy |level|, pairing will be re-initiated. Note that this means
  // security requests of different |level|s may not complete in the order they are made.
  //
  // If no pairing is in progress then the local device will initiate pairing.
  //
  // If pairing fails |callback| will be called with a |status| that represents the error.
  using PairingCallback = fit::function<void(Result<> status, const SecurityProperties& sec_props)>;
  virtual void UpgradeSecurity(SecurityLevel level, PairingCallback callback) = 0;

  // Assign I/O capabilities. This aborts any ongoing pairing procedure and sets
  // up the I/O capabilities to use for future requests.
  virtual void Reset(IOCapability io_capability) = 0;

  // Abort all ongoing pairing procedures and notify pairing callbacks with the provided error.
  void Abort() { Abort(ErrorCode::kUnspecifiedReason); }
  virtual void Abort(ErrorCode ecode) = 0;

  // Returns the current security properties of the LE link.
  const SecurityProperties& security() const { return le_sec_; }

  // Returns whether or not the SecurityManager is in bondable mode. Note that being in bondable
  // mode does not guarantee that pairing will necessarily bond.
  BondableMode bondable_mode() const { return bondable_mode_; }

  // Sets the bondable mode of the SecurityManager. Any in-progress pairings will not be affected -
  // if bondable mode needs to be reset during a pairing Reset() or Abort() must be called first.
  void set_bondable_mode(sm::BondableMode mode) { bondable_mode_ = mode; }

  // Sets the LE Security mode of the SecurityManager - see enum definition for details of each
  // mode. If a security upgrade is in-progress, only takes effect on the next security upgrade.
  void set_security_mode(gap::LESecurityMode mode) { security_mode_ = mode; }
  gap::LESecurityMode security_mode() { return security_mode_; }

 protected:
  SecurityManager(BondableMode bondable_mode, gap::LESecurityMode security_mode);
  void set_security(SecurityProperties security) { le_sec_ = security; }

 private:
  // The operating bondable mode of the device.
  BondableMode bondable_mode_ = BondableMode::Bondable;

  // The current GAP security mode of the device (v5.2 Vol. 3 Part C Section 10.2)
  gap::LESecurityMode security_mode_ = gap::LESecurityMode::Mode1;

  // Current security properties of the LE-U link.
  SecurityProperties le_sec_ = SecurityProperties();
};

using SecurityManagerFactory = std::function<decltype(sm::SecurityManager::Create)>;

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SECURITY_MANAGER_H_
