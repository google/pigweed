// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_TEST_SECURITY_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_TEST_SECURITY_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/error.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm::testing {

// TestSecurityManager implements the public interface of the SM library. The intended use is in
// unit tests of code directly dependent on SM (currently, GAP). The implementation is currently
// limited to a basic test spy, with stubbed out responses and request tracking for a few functions
// and empty implementations for others.
class TestSecurityManager final : public SecurityManager {
 public:
  ~TestSecurityManager() = default;

  // SecurityManager overrides:
  bool AssignLongTermKey(const LTK& ltk) override;
  void UpgradeSecurity(SecurityLevel level, PairingCallback callback) override;
  void Reset(IOCapability io_capability) override;
  void Abort(ErrorCode ecode) override;

  // Returns the most recent security upgrade request received by this SM, if one has been made.
  std::optional<SecurityLevel> last_requested_upgrade() const { return last_requested_upgrade_; }

  using WeakPtr = WeakSelf<TestSecurityManager>::WeakPtr;
  WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  friend class TestSecurityManagerFactory;
  TestSecurityManager(hci::LowEnergyConnection::WeakPtr link, l2cap::Channel::WeakPtr smp,
                      IOCapability io_capability, Delegate::WeakPtr delegate,
                      BondableMode bondable_mode, gap::LESecurityMode security_mode);
  Role role_;
  std::optional<LTK> current_ltk_;
  std::optional<SecurityLevel> last_requested_upgrade_;
  WeakSelf<TestSecurityManager> weak_self_;
};

// TestSecurityManagerFactory provides a public factory method to create TestSecurityManagers for
// dependency injection. It stores these TestSMs so test code can access them to set and verify
// expectations. A separate storage object is needed because SecurityManagers cannot be directly
// injected, e.g. during construction, as they are created on demand upon connection creation.
// Storing the TestSMs in a factory object is preferable to a static member of TestSM itself so
// that each test is sandboxed from TestSMs in other tests. This is done by tying the lifetime of
// the factory to the test.
class TestSecurityManagerFactory {
 public:
  TestSecurityManagerFactory() = default;
  // Code which uses TestSecurityManagers should create these objects through `CreateSm`.
  // |link|: The LE logical link over which pairing procedures occur.
  // |smp|: The L2CAP LE SMP fixed channel that operates over |link|.
  // |io_capability|: The initial I/O capability.
  // |delegate|: Delegate which handles SMP interactions with the rest of the Bluetooth stack.
  // |bondable_mode|: the operating bondable mode of the device (see v5.2, Vol. 3, Part C 9.4).
  // |security_mode|: the security mode this SecurityManager is in (see v5.2, Vol. 3, Part C 10.2).
  std::unique_ptr<SecurityManager> CreateSm(hci::LowEnergyConnection::WeakPtr link,
                                            l2cap::Channel::WeakPtr smp, IOCapability io_capability,
                                            Delegate::WeakPtr delegate, BondableMode bondable_mode,
                                            gap::LESecurityMode security_mode);

  // Obtain a reference to the TestSecurityManager associated with |conn_handle|'s connection for
  // use in test code.
  testing::TestSecurityManager::WeakPtr GetTestSm(hci_spec::ConnectionHandle conn_handle);

 private:
  std::unordered_map<hci_spec::ConnectionHandle, testing::TestSecurityManager::WeakPtr> test_sms_;
};

}  // namespace bt::sm::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_TEST_SECURITY_MANAGER_H_
