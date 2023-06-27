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

#include <cinttypes>
#include <mutex>
#include <optional>

#include "fsl_spi.h"
#include "pw_spi/chip_selector.h"
#include "pw_spi/initiator.h"
#include "pw_status/status.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::spi {

// Mcuxpresso SDK implementation of the SPI Initiator
class McuxpressoInitiator : public Initiator {
 public:
  McuxpressoInitiator(SPI_Type* register_map,
                      uint32_t max_speed_hz,
                      uint32_t baud_rate_bps,
                      bool blocking = true)
      : register_map_(register_map),
        max_speed_hz_(max_speed_hz),
        baud_rate_bps_(baud_rate_bps),
        blocking_(blocking) {}
  ~McuxpressoInitiator();

  // Implements pw::spi::Initiator
  Status Configure(const Config& config) PW_LOCKS_EXCLUDED(mutex_) override;
  Status WriteRead(ConstByteSpan write_buffer, ByteSpan read_buffer)
      PW_LOCKS_EXCLUDED(mutex_) override;

  Status SetChipSelect(uint32_t pin) PW_LOCKS_EXCLUDED(mutex_);

 private:
  // inclusive-language: disable
  static void SpiCallback(SPI_Type* base,
                          spi_master_handle_t* driver_handle,
                          status_t status,
                          void* context);

  Status DoConfigure(const Config& config,
                     const std::lock_guard<sync::Mutex>& lock);

  bool is_initialized() { return !!current_config_; }

  SPI_Type* register_map_;
  spi_master_handle_t driver_handle_;
  // inclusive-language: enable
  sync::BinarySemaphore transfer_semaphore_;
  sync::Mutex mutex_;
  Status last_transfer_status_;
  uint32_t max_speed_hz_;
  uint32_t baud_rate_bps_;
  bool blocking_;
  std::optional<const Config> current_config_;
  uint32_t pin_ = 0;
};

// Mcuxpresso userspace implementation of SPI ChipSelector
// NOTE: This implementation deviates from the expected for this interface.
// It only specifies which chipselect pin should be activated and does not
// activate the pin itself. Activation of the pin is handled at a lower level by
// the Mcuxpresso vendor driver.
// This chipselector may only be used with a single McuxpressoInitiator
class McuxpressoChipSelector : public ChipSelector {
 public:
  McuxpressoChipSelector(McuxpressoInitiator& initiator, uint32_t pin)
      : initiator_(initiator), pin_(pin) {}

  // Implements pw::spi::ChipSelector
  // Instead of directly activating the cs line, this informs the underlying
  // driver to do so.
  Status SetActive(bool active) override {
    return initiator_.SetChipSelect(pin_);
  }

 private:
  McuxpressoInitiator& initiator_;
  uint32_t pin_;
};

}  // namespace pw::spi
