// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth/low_energy/peripheral2.h"
#include "pw_bluetooth_sapphire/internal/host/gap/adapter.h"

namespace pw::bluetooth_sapphire {

/// Must only be constructed and destroyed on the Bluetooth thread.
class Peripheral final : public pw::bluetooth::low_energy::Peripheral2 {
 public:
  // TODO: https://pwbug.dev/377301546 - Don't expose Adapter in public API.
  /// Must only be constructed on the Bluetooth thread.
  Peripheral(bt::gap::Adapter::WeakPtr adapter,
             pw::async::Dispatcher& dispatcher);

  /// Must only be destroyed on the Bluetooth thread.
  ~Peripheral() override;

  /// Thread safe.
  /// The AdvertisedPeripheral2 returned on success is thread safe.
  async2::OnceReceiver<AdvertiseResult> Advertise(
      const AdvertisingParameters& parameters) override;

 private:
  // Thin client handle for an active advertisement.
  // Thread safe.
  class AdvertisedPeripheralImpl final
      : public pw::bluetooth::low_energy::AdvertisedPeripheral2 {
   public:
    AdvertisedPeripheralImpl(bt::gap::AdvertisementId id) : id_(id) {}
    ~AdvertisedPeripheralImpl() override {
      // TODO: https://pwbug.dev/377301546 - Implement advertisement stopping.
    }

    void Release() override { delete this; }

   private:
    // AdvertisedPeripheral2 overrides:
    async2::Poll<pw::bluetooth::low_energy::Connection2::Ptr> PendConnection(
        async2::Context& cx) override {
      // TODO: https://pwbug.dev/377301546 - Implement connection handling.
      return async2::Pending();
    }
    void StopAdvertising() override {
      // TODO: https://pwbug.dev/377301546 - Implement advertisement stopping.
    }
    async2::Poll<pw::Status> PendStop(async2::Context& cx) override {
      // TODO: https://pwbug.dev/377301546 - Implement advertisement stopping.
      return async2::Pending();
    }

    [[maybe_unused]] bt::gap::AdvertisementId id_;
  };

  // Active advertisement state.
  class Advertisement final {
   public:
    Advertisement(bt::gap::AdvertisementInstance&& instance,
                  AdvertisedPeripheralImpl* advertised_peripheral)
        : advertised_peripheral_(advertised_peripheral),
          instance_(std::move(instance)) {}

   private:
    [[maybe_unused]] AdvertisedPeripheralImpl* advertised_peripheral_;
    bt::gap::AdvertisementInstance instance_;
  };

  // Always called on Bluetooth thread.
  void OnAdvertiseResult(bt::gap::AdvertisementInstance instance,
                         bt::hci::Result<> result,
                         async2::OnceSender<AdvertiseResult> result_sender);

  // Always called on Bluetooth thread.
  void OnConnection(bt::gap::AdvertisementId advertisement_id,
                    bt::gap::Adapter::LowEnergy::ConnectionResult result);

  // Dispatcher for Bluetooth thread.
  pw::async::HeapDispatcher dispatcher_;

  // Must only be used on the Bluetooth thread.
  bt::gap::Adapter::WeakPtr adapter_;

  // Must only be used on the Bluetooth thread.
  std::unordered_map<bt::gap::AdvertisementId, Advertisement> advertisements_;

  // Must only be used on the Bluetooth thread.
  WeakSelf<Peripheral> weak_factory_{this};

  // Thread safe to copy and destroy, but WeakPtr should only be used
  // or dereferenced on the Bluetooth thread (WeakRef is not thread safe).
  WeakSelf<Peripheral>::WeakPtr self_{weak_factory_.GetWeakPtr()};
};

}  // namespace pw::bluetooth_sapphire
