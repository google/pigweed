// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_ADVERTISING_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_ADVERTISING_MANAGER_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_advertiser.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace hci {
class Connection;
class LocalAddressDelegate;
class Transport;
}  // namespace hci

namespace gap {

using AdvertisementId = Identifier<uint64_t>;
constexpr AdvertisementId kInvalidAdvertisementId(0u);

class LowEnergyAdvertisingManager;

// Represents an active advertising instance. Stops the associated advertisement upon destruction.
class AdvertisementInstance final {
 public:
  // The default constructor initializes an instance with an invalid ID.
  AdvertisementInstance();
  ~AdvertisementInstance();

  AdvertisementInstance(AdvertisementInstance&&) = default;
  AdvertisementInstance& operator=(AdvertisementInstance&&) = default;

  AdvertisementId id() const { return id_; }

 private:
  friend class LowEnergyAdvertisingManager;

  AdvertisementInstance(AdvertisementId id, fxl::WeakPtr<LowEnergyAdvertisingManager> owner);

  AdvertisementId id_;
  fxl::WeakPtr<LowEnergyAdvertisingManager> owner_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisementInstance);
};

// Enum values for determining the advertising interval range. These ranges come from Core
// Specification v5.1, Vol 3, Part C, Appendix A (see also the constants defined in gap.h).
enum class AdvertisingInterval {
  FAST1,
  FAST2,
  SLOW,
};

class LowEnergyAdvertisingManager {
 public:
  LowEnergyAdvertisingManager(hci::LowEnergyAdvertiser* advertiser,
                              hci::LocalAddressDelegate* local_addr_delegate);
  virtual ~LowEnergyAdvertisingManager();

  // Asynchronously attempts to start advertising a set of |data| with
  // additional scan response data |scan_rsp|.
  //
  // If |connect_callback| is provided, the advertisement will be connectable
  // and it will be called with the returned advertisement_id and a pointer to
  // the new connection, at which point the advertisement will have been
  // stopped.
  //
  // Returns false if the parameters represent an invalid advertisement:
  //  * if |anonymous| is true but |callback| is set
  //
  // |status_callback| may be called synchronously within this function.
  // |status_callback| provides one of:
  //  - an |advertisement_id|, which can be used to stop advertising
  //    or disambiguate calls to |callback|, and a success |status|.
  //  - kInvalidAdvertisementId and an error indication in |status|:
  //    * HostError::kInvalidParameters if the advertising parameters
  //      are invalid (e.g. |data| is too large).
  //    * HostError::kNotSupported if another set cannot be advertised
  //      or if the requested parameters are not supported by the hardware.
  //    * HostError::kProtocolError with a HCI error reported from
  //      the controller, otherwise.
  using ConnectionCallback =
      fit::function<void(AdvertisementId advertisement_id, std::unique_ptr<hci::Connection> link)>;
  using AdvertisingStatusCallback =
      fit::function<void(AdvertisementInstance instance, hci::Status status)>;
  void StartAdvertising(const AdvertisingData& data, const AdvertisingData& scan_rsp,
                        ConnectionCallback connect_callback, AdvertisingInterval interval,
                        bool anonymous, AdvertisingStatusCallback status_callback);

  // Stop advertising the advertisement with the id |advertisement_id|
  // Returns true if an advertisement was stopped, and false otherwise.
  // This function is idempotent.
  bool StopAdvertising(AdvertisementId advertisement_id);

 private:
  class ActiveAdvertisement;

  // Active advertisements, indexed by id.
  // TODO(armansito): Use fbl::HashMap here (NET-176) or move
  // ActiveAdvertisement definition here and store by value (it is a small
  // object).
  std::unordered_map<AdvertisementId, std::unique_ptr<ActiveAdvertisement>> advertisements_;

  // Used to communicate with the controller. |advertiser_| must outlive this
  // advertising manager.
  hci::LowEnergyAdvertiser* advertiser_;  // weak

  // Used to obtain the local device address for advertising. Must outlive this
  // advertising manager.
  hci::LocalAddressDelegate* local_addr_delegate_;  // weak

  // Note: Should remain the last member so it'll be destroyed and
  // invalidate it's pointers before other members are destroyed.
  fxl::WeakPtrFactory<LowEnergyAdvertisingManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyAdvertisingManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_ADVERTISING_MANAGER_H_
