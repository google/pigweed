// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_advertising_manager.h"

#include <zircon/assert.h>

#include "low_energy_address_manager.h"
#include "remote_device.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace gap {

namespace {

constexpr size_t kFlagsSize = 3;
constexpr uint8_t kDefaultFlags = 0;

// Write the block for the flags to the |buffer|.
void WriteFlags(common::MutableByteBuffer* buffer, bool limited = false) {
  ZX_DEBUG_ASSERT(buffer->size() >= kFlagsSize);
  (*buffer)[0] = 2;
  (*buffer)[1] = static_cast<uint8_t>(DataType::kFlags);
  if (limited) {
    (*buffer)[2] = kDefaultFlags | AdvFlag::kLELimitedDiscoverableMode;
  } else {
    (*buffer)[2] = kDefaultFlags | AdvFlag::kLEGeneralDiscoverableMode;
  }
}

}  // namespace

class LowEnergyAdvertisingManager::ActiveAdvertisement final {
 public:
  // TODO(BT-270): Don't randomly generate the ID of an advertisement.
  // Instead use a counter like other internal IDs once this ID is not visible
  // outside of bt-host.
  explicit ActiveAdvertisement(const common::DeviceAddress& address)
      : address_(address), id_(common::RandomDeviceId().value()) {}

  ~ActiveAdvertisement() = default;

  const common::DeviceAddress& address() const { return address_; }
  AdvertisementId id() const { return id_; }

 private:
  common::DeviceAddress address_;
  AdvertisementId id_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ActiveAdvertisement);
};

LowEnergyAdvertisingManager::LowEnergyAdvertisingManager(
    hci::LowEnergyAdvertiser* advertiser,
    hci::LocalAddressDelegate* local_addr_delegate)
    : advertiser_(advertiser),
      local_addr_delegate_(local_addr_delegate),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(advertiser_);
  ZX_DEBUG_ASSERT(local_addr_delegate_);
}

LowEnergyAdvertisingManager::~LowEnergyAdvertisingManager() {
  // Turn off all the advertisements!
  for (const auto& ad : advertisements_) {
    advertiser_->StopAdvertising(ad.second->address());
  }
}

void LowEnergyAdvertisingManager::StartAdvertising(
    const AdvertisingData& data, const AdvertisingData& scan_rsp,
    ConnectionCallback connect_callback, zx::duration interval, bool anonymous,
    AdvertisingStatusCallback status_callback) {
  // Can't be anonymous and connectable
  if (anonymous && connect_callback) {
    bt_log(TRACE, "gap-le", "can't advertise anonymously and connectable!");
    status_callback(kInvalidAdvertisementId,
                    hci::Status(common::HostError::kInvalidParameters));
    return;
  }

  // Serialize the data
  auto data_bytes =
      common::NewSlabBuffer(data.CalculateBlockSize() + kFlagsSize);
  WriteFlags(data_bytes.get());
  auto data_view = data_bytes->mutable_view(kFlagsSize);
  data.WriteBlock(&data_view);

  auto scan_rsp_bytes = common::NewSlabBuffer(scan_rsp.CalculateBlockSize());
  scan_rsp.WriteBlock(scan_rsp_bytes.get());

  auto self = weak_ptr_factory_.GetWeakPtr();

  // TODO(BT-742): The address used for legacy advertising must be
  // coordinated via |local_addr_delegate_| however a unique address can be
  // generated and assigned to each advertising set when the controller
  // supports 5.0 extended advertising. hci::LowEnergyAdvertiser needs to be
  // revised to not use device addresses to distinguish between advertising
  // instances especially since |local_addr_delegate_| is likely to return the
  // same address if called frequently.
  //
  // Revisit this logic when multi-advertising is supported.
  local_addr_delegate_->EnsureLocalAddress(
      [self, interval, anonymous, data_bytes = std::move(data_bytes),
       scan_rsp_bytes = std::move(scan_rsp_bytes),
       connect_cb = std::move(connect_callback),
       status_cb = std::move(status_callback)](const auto& address) mutable {
        if (!self)
          return;

        auto ad_ptr = std::make_unique<ActiveAdvertisement>(address);
        hci::LowEnergyAdvertiser::ConnectionCallback adv_conn_cb;
        if (connect_cb) {
          adv_conn_cb = [self, id = ad_ptr->id(),
                         connect_cb = std::move(connect_cb)](auto link) {
            bt_log(TRACE, "gap-le", "received new connection");

            if (!self)
              return;

            // remove the advertiser because advertising has stopped
            self->advertisements_.erase(id);
            connect_cb(id, std::move(link));
          };
        }
        auto status_cb_wrapper = [self, ad_ptr = std::move(ad_ptr),
                                  status_cb = std::move(status_cb)](
                                     zx::duration, hci::Status status) mutable {
          if (!self)
            return;

          if (!status) {
            status_cb(kInvalidAdvertisementId, status);
            return;
          }

          auto id = ad_ptr->id();
          self->advertisements_.emplace(id, std::move(ad_ptr));
          status_cb(id, status);
        };

        // Call StartAdvertising, with the callback
        self->advertiser_->StartAdvertising(
            address, *data_bytes, *scan_rsp_bytes, std::move(adv_conn_cb),
            interval, anonymous, std::move(status_cb_wrapper));
      });
}

bool LowEnergyAdvertisingManager::StopAdvertising(
    AdvertisementId advertisement_id) {
  auto it = advertisements_.find(advertisement_id);
  if (it == advertisements_.end())
    return false;

  advertiser_->StopAdvertising(it->second->address());
  advertisements_.erase(it);
  return true;
}

}  // namespace gap
}  // namespace bt
