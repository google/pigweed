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

#include "pw_spi_rp2040/initiator.h"

#include <algorithm>

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

namespace pw::spi {

namespace {

constexpr spi_order_t GetBitOrder(BitOrder bit_order) {
  switch (bit_order) {
    case BitOrder::kLsbFirst:
      return SPI_LSB_FIRST;
    case BitOrder::kMsbFirst:
      return SPI_MSB_FIRST;
    default:
      PW_CRASH("Unknown bit order");
  }
}

constexpr spi_cpha_t GetPhase(ClockPhase phase) {
  switch (phase) {
    case ClockPhase::kRisingEdge:
      return SPI_CPHA_0;
    case ClockPhase::kFallingEdge:
      return SPI_CPHA_1;
    default:
      PW_CRASH("Unknown phase");
  }
}

constexpr spi_cpol_t GetPolarity(ClockPolarity polarity) {
  switch (polarity) {
    case ClockPolarity::kActiveHigh:
      return SPI_CPOL_0;
    case ClockPolarity::kActiveLow:
      return SPI_CPOL_1;
    default:
      PW_CRASH("Unknown polarity");
  }
}

}  // namespace

Rp2040Initiator::Rp2040Initiator(spi_inst_t* spi)
    : spi_(spi), bits_per_word_(8) {}

Status Rp2040Initiator::LazyInit() {
  // Already initialized - nothing to do.
  // The Pico SDK needs to call spi_init() earlier so that the
  // various GPIO pins (MISO, etc.) can be assigned to the SPI
  // bus.
  return OkStatus();
}

Status Rp2040Initiator::Configure(const Config& config) {
  bits_per_word_ = config.bits_per_word;
  PW_ASSERT(bits_per_word_() == 8);

  spi_set_format(spi_,
                 config.bits_per_word(),
                 GetPolarity(config.polarity),
                 GetPhase(config.phase),
                 GetBitOrder(config.bit_order));

  return OkStatus();
}

Status Rp2040Initiator::WriteRead(ConstByteSpan write_buffer,
                                  ByteSpan read_buffer) {
  if (write_buffer.empty() && !read_buffer.empty()) {
    // Read only transaction.
    spi_read_blocking(spi_,
                      /*repeated_tx_data=*/0,
                      reinterpret_cast<uint8_t*>(read_buffer.data()),
                      read_buffer.size());
  } else if (!write_buffer.empty() && read_buffer.empty()) {
    // Write only transaction.
    spi_write_blocking(spi_,
                       reinterpret_cast<const uint8_t*>(write_buffer.data()),
                       write_buffer.size());
  } else if (!write_buffer.empty() && !read_buffer.empty()) {
    // Write & read transaction.
    // Take the smallest as the size of transaction
    auto transfer_size = write_buffer.size() < read_buffer.size()
                             ? write_buffer.size()
                             : read_buffer.size();
    spi_write_read_blocking(
        spi_,
        reinterpret_cast<const uint8_t*>(write_buffer.data()),
        reinterpret_cast<uint8_t*>(read_buffer.data()),
        transfer_size);
  } else {
    return pw::Status::OutOfRange();
  }

  return OkStatus();
}

}  // namespace pw::spi
