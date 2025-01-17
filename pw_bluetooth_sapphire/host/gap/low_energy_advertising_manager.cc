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

#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_advertising_manager.h"

#include <cpp-string/string_printf.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/common/random.h"
#include "pw_bluetooth_sapphire/internal/host/common/slab_allocator.h"
#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_address_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gap/peer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"

namespace bt::gap {

namespace {

// Returns the matching minimum and maximum advertising interval values in
// controller timeslices.
hci::AdvertisingIntervalRange GetIntervalRange(AdvertisingInterval interval) {
  switch (interval) {
    case AdvertisingInterval::FAST1:
      return {kLEAdvertisingFastIntervalMin1, kLEAdvertisingFastIntervalMax1};
    case AdvertisingInterval::FAST2:
      return {kLEAdvertisingFastIntervalMin2, kLEAdvertisingFastIntervalMax2};
    case AdvertisingInterval::SLOW:
      return {kLEAdvertisingSlowIntervalMin, kLEAdvertisingSlowIntervalMax};
  }

  PW_CRASH("unexpected advertising interval value");
  return {kLEAdvertisingSlowIntervalMin, kLEAdvertisingSlowIntervalMax};
}

}  // namespace

AdvertisementInstance::AdvertisementInstance() : id_(kInvalidAdvertisementId) {}

AdvertisementInstance::AdvertisementInstance(
    AdvertisementId advertisement_id,
    fit::function<void(AdvertisementId)> stop_advertising)
    : id_(advertisement_id), stop_advertising_(std::move(stop_advertising)) {
  PW_DCHECK(id_ != kInvalidAdvertisementId);
  PW_DCHECK(stop_advertising_ != nullptr);
}

AdvertisementInstance::~AdvertisementInstance() { Reset(); }

void AdvertisementInstance::Move(AdvertisementInstance* other) {
  // Destroy the old advertisement instance if active and clear the contents.
  Reset();

  // Transfer the old data over and clear |other| so that it no longer owns its
  // advertisement.
  stop_advertising_ = std::move(other->stop_advertising_);
  id_ = other->id_;
  other->id_ = kInvalidAdvertisementId;
}

void AdvertisementInstance::Reset() {
  if (stop_advertising_ && id_ != kInvalidAdvertisementId) {
    stop_advertising_(id_);
  }

  stop_advertising_ = nullptr;
  id_ = kInvalidAdvertisementId;
}

class LowEnergyAdvertisingManager::ActiveAdvertisement final {
 public:
  explicit ActiveAdvertisement(const DeviceAddress& address,
                               AdvertisementId id,
                               bool extended_pdu)
      : address_(address), id_(id), extended_pdu_(extended_pdu) {}

  ~ActiveAdvertisement() = default;

  const DeviceAddress& address() const { return address_; }
  AdvertisementId id() const { return id_; }
  bool extended_pdu() const { return extended_pdu_; }

 private:
  DeviceAddress address_;
  AdvertisementId id_;
  bool extended_pdu_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ActiveAdvertisement);
};

LowEnergyAdvertisingManager::LowEnergyAdvertisingManager(
    hci::LowEnergyAdvertiser* advertiser,
    hci::LocalAddressDelegate* local_addr_delegate)
    : advertiser_(advertiser),
      local_addr_delegate_(local_addr_delegate),
      weak_self_(this) {
  PW_DCHECK(advertiser_);
  PW_DCHECK(local_addr_delegate_);
}

LowEnergyAdvertisingManager::~LowEnergyAdvertisingManager() {
  // Turn off all the advertisements!
  for (const auto& ad : advertisements_) {
    advertiser_->StopAdvertising(ad.second->address(),
                                 ad.second->extended_pdu());
  }
}

void LowEnergyAdvertisingManager::StartAdvertising(
    AdvertisingData data,
    AdvertisingData scan_rsp,
    ConnectionCallback connect_callback,
    AdvertisingInterval interval,
    bool extended_pdu,
    bool anonymous,
    bool include_tx_power_level,
    std::optional<DeviceAddress::Type> address_type,
    AdvertisingStatusCallback status_callback) {
  // Can't be anonymous and connectable
  if (anonymous && connect_callback) {
    bt_log(DEBUG, "gap-le", "can't advertise anonymously and connectable!");
    status_callback(AdvertisementInstance(),
                    ToResult(HostError::kInvalidParameters));
    return;
  }

  // v5.1, Vol 3, Part C, Appendix A recommends the FAST1 parameters for
  // connectable advertising and FAST2 parameters for non-connectable
  // advertising. Some Bluetooth controllers reject the FAST1 parameters for
  // non-connectable advertising, hence we fall back to FAST2 in that case.
  if (interval == AdvertisingInterval::FAST1 && !connect_callback) {
    interval = AdvertisingInterval::FAST2;
  }
  hci::LowEnergyAdvertiser::AdvertisingOptions options(
      GetIntervalRange(interval),
      AdvFlag::kLEGeneralDiscoverableMode,
      extended_pdu,
      anonymous,
      include_tx_power_level);

  auto self = weak_self_.GetWeakPtr();

  // TODO: https://fxbug.dev/42083437 - The address used for legacy advertising
  // must be coordinated via |local_addr_delegate_| however a unique address can
  // be generated and assigned to each advertising set when the controller
  // supports 5.0 extended advertising. hci::LowEnergyAdvertiser needs to be
  // revised to not use device addresses to distinguish between advertising
  // instances especially since |local_addr_delegate_| is likely to return the
  // same address if called frequently.
  //
  // Revisit this logic when multi-advertising is supported.
  local_addr_delegate_->EnsureLocalAddress(
      address_type,
      [self,
       advertising_data = std::move(data),
       scan_response = std::move(scan_rsp),
       options,
       connect_cb = std::move(connect_callback),
       status_cb = std::move(status_callback)](
          fit::result<HostError, const DeviceAddress> result) mutable {
        if (!self.is_alive()) {
          return;
        }

        if (result.is_error()) {
          status_cb(AdvertisementInstance(), fit::error(result.error_value()));
          return;
        }

        auto ad_ptr = std::make_unique<ActiveAdvertisement>(
            result.value(),
            AdvertisementId(self->next_advertisement_id_++),
            options.extended_pdu);
        hci::LowEnergyAdvertiser::ConnectionCallback adv_conn_cb;
        if (connect_cb) {
          adv_conn_cb = [self,
                         id = ad_ptr->id(),
                         on_connect_cb = std::move(connect_cb)](auto link) {
            bt_log(DEBUG, "gap-le", "received new connection");

            if (!self.is_alive()) {
              return;
            }

            // remove the advertiser because advertising has stopped
            self->advertisements_.erase(id);
            on_connect_cb(id, std::move(link));
          };
        }
        auto status_cb_wrapper = [self,
                                  advertisement_ptr = std::move(ad_ptr),
                                  result_cb = std::move(status_cb)](
                                     hci::Result<> status) mutable {
          if (!self.is_alive()) {
            return;
          }

          if (status.is_error()) {
            result_cb(AdvertisementInstance(), status);
            return;
          }

          auto id = advertisement_ptr->id();
          self->advertisements_.emplace(id, std::move(advertisement_ptr));
          auto stop_advertising_cb = [self](AdvertisementId stop_id) {
            if (self.is_alive()) {
              self->StopAdvertising(stop_id);
            }
          };
          result_cb(AdvertisementInstance(id, std::move(stop_advertising_cb)),
                    status);
        };

        // Call StartAdvertising, with the callback
        self->advertiser_->StartAdvertising(result.value(),
                                            advertising_data,
                                            scan_response,
                                            options,
                                            std::move(adv_conn_cb),
                                            std::move(status_cb_wrapper));
      });
}

bool LowEnergyAdvertisingManager::StopAdvertising(
    AdvertisementId advertisement_id) {
  auto it = advertisements_.find(advertisement_id);
  if (it == advertisements_.end()) {
    return false;
  }

  advertiser_->StopAdvertising(it->second->address(),
                               it->second->extended_pdu());
  advertisements_.erase(it);
  return true;
}

}  // namespace bt::gap
