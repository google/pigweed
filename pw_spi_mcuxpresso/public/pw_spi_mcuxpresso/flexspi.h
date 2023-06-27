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

#include "fsl_flexio_spi.h"
#include "pw_digital_io/digital_io.h"
#include "pw_spi/chip_selector.h"
#include "pw_spi/initiator.h"
#include "pw_status/status.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::spi {

// Mcuxpresso SDK implementation of the FLEXIO SPI Initiator
class McuxpressoFlexIoInitiator : public Initiator {
 public:
  McuxpressoFlexIoInitiator(FLEXIO_SPI_Type flexio_spi_config,
                            uint32_t src_clock_hz,
                            uint32_t baud_rate_bps,
                            bool blocking = true)
      : flexio_spi_config_(flexio_spi_config),
        src_clock_hz_(src_clock_hz),
        baud_rate_bps_(baud_rate_bps),
        blocking_(blocking) {}
  ~McuxpressoFlexIoInitiator();

  // Implements pw::spi::Initiator
  pw::Status Configure(const Config& config) PW_LOCKS_EXCLUDED(mutex_) override;
  pw::Status WriteRead(ConstByteSpan write_buffer, ByteSpan read_buffer)
      PW_LOCKS_EXCLUDED(mutex_) override;

 private:
  // inclusive-language: disable
  static void SpiCallback(FLEXIO_SPI_Type*,
                          flexio_spi_master_handle_t*,
                          status_t status,
                          void* context);
  // Add support to FLEXIO_SPI for negative clock polarity.
  void ConfigureClock(flexio_spi_master_config_t* masterConfig,
                      ClockPolarity clockPolarity);

  bool is_initialized() { return !!current_config_; }

  std::optional<const Config> current_config_;
  FLEXIO_SPI_Type flexio_spi_config_;
  flexio_spi_master_handle_t driver_handle_;
  // inclusive-language: enable
  sync::BinarySemaphore transfer_semaphore_;
  sync::Mutex mutex_;
  Status last_transfer_status_;
  uint32_t src_clock_hz_;
  uint32_t baud_rate_bps_;
  bool blocking_;
  uint8_t transfer_flags_;
};

// Mcuxpresso userspace implementation of SPI ChipSelector. Implemented using
// GPIO so as to support manual control of chip select. GPIO pin passed in
// should be already initialized and ungated.
class McuxpressoFlexIoChipSelector : public ChipSelector {
 public:
  explicit McuxpressoFlexIoChipSelector(digital_io::DigitalOut& pin)
      : pin_(pin) {}

  // Implements pw::spi::ChipSelector
  pw::Status SetActive(bool active) override;

 private:
  digital_io::DigitalOut& pin_;
};

}  // namespace pw::spi
