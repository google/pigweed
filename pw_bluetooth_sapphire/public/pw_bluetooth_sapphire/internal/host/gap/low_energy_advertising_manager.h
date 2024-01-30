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

#pragma once
#include <lib/fit/function.h>

#include "pw_bluetooth_sapphire/internal/host/common/advertising_data.h"
#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/gap/gap.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_advertiser.h"
#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_connection.h"

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

// Represents an active advertising instance. Stops the associated advertisement
// upon destruction.
class AdvertisementInstance final {
 public:
  // The default constructor initializes an instance with an invalid ID.
  AdvertisementInstance();
  ~AdvertisementInstance();

  AdvertisementInstance(AdvertisementInstance&& other) { Move(&other); }
  AdvertisementInstance& operator=(AdvertisementInstance&& other) {
    Move(&other);
    return *this;
  }

  AdvertisementId id() const { return id_; }

 private:
  friend class LowEnergyAdvertisingManager;

  AdvertisementInstance(AdvertisementId id,
                        WeakSelf<LowEnergyAdvertisingManager>::WeakPtr owner);
  void Move(AdvertisementInstance* other);
  void Reset();

  AdvertisementId id_;
  WeakSelf<LowEnergyAdvertisingManager>::WeakPtr owner_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisementInstance);
};

// Enum values for determining the advertising interval range. These ranges come
// from Core Specification v5.1, Vol 3, Part C, Appendix A (see also the
// constants defined in gap.h).
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

  // Returns true if the controller is currently advertising.
  bool advertising() const { return !advertisements_.empty(); }

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
      fit::function<void(AdvertisementId advertisement_id,
                         std::unique_ptr<hci::LowEnergyConnection> link)>;
  using AdvertisingStatusCallback =
      fit::function<void(AdvertisementInstance instance, hci::Result<> status)>;
  void StartAdvertising(AdvertisingData data,
                        AdvertisingData scan_rsp,
                        ConnectionCallback connect_callback,
                        AdvertisingInterval interval,
                        bool anonymous,
                        bool include_tx_power_level,
                        AdvertisingStatusCallback status_callback);

  // Stop advertising the advertisement with the id |advertisement_id|
  // Returns true if an advertisement was stopped, and false otherwise.
  // This function is idempotent.
  bool StopAdvertising(AdvertisementId advertisement_id);

 private:
  class ActiveAdvertisement;

  // 0 is invalid, so we start at 1.
  AdvertisementId::value_t next_advertisement_id_ = 1;

  // Active advertisements, indexed by id.
  // TODO(armansito): Use fbl::HashMap here (fxbug.dev/42143883) or move
  // ActiveAdvertisement definition here and store by value (it is a small
  // object).
  std::unordered_map<AdvertisementId, std::unique_ptr<ActiveAdvertisement>>
      advertisements_;

  // Used to communicate with the controller. |advertiser_| must outlive this
  // advertising manager.
  hci::LowEnergyAdvertiser* advertiser_;  // weak

  // Used to obtain the local device address for advertising. Must outlive this
  // advertising manager.
  hci::LocalAddressDelegate* local_addr_delegate_;  // weak

  // Note: Should remain the last member so it'll be destroyed and
  // invalidate it's pointers before other members are destroyed.
  WeakSelf<LowEnergyAdvertisingManager> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyAdvertisingManager);
};

}  // namespace gap
}  // namespace bt
