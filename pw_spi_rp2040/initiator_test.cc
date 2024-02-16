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

#include <gtest/gtest.h>

#include "hardware/spi.h"
#include "pw_digital_io/polarity.h"
#include "pw_digital_io_rp2040/digital_io.h"
#include "pw_spi/chip_selector_digital_out.h"
#include "pw_spi/device.h"
#include "pw_sync/borrow.h"
#include "pw_sync/mutex.h"

using pw::spi::DigitalOutChipSelector;
using pw::spi::Initiator;
using pw::spi::Rp2040Initiator;

namespace pw::spi {
namespace {

constexpr pw::spi::Config kSpiConfig8Bit{
    .polarity = pw::spi::ClockPolarity::kActiveHigh,
    .phase = pw::spi::ClockPhase::kFallingEdge,
    .bits_per_word = pw::spi::BitsPerWord(8),
    .bit_order = pw::spi::BitOrder::kMsbFirst,
};

TEST(InitiatorTest, Init) {
  // Simple test only meant to ensure module is compiled.
  pw::digital_io::Rp2040DigitalInOut display_cs_pin({
      .pin = 12,
      .polarity = pw::digital_io::Polarity::kActiveLow,
  });
  DigitalOutChipSelector spi_chip_selector(display_cs_pin);
  Rp2040Initiator spi_initiator(spi0);

  pw::sync::VirtualMutex initiator_mutex;
  pw::sync::Borrowable<pw::spi::Initiator> borrowable_initiator(
      spi_initiator, initiator_mutex);
  pw::spi::Device spi_8_bit(
      borrowable_initiator, kSpiConfig8Bit, spi_chip_selector);
}

}  // namespace
}  // namespace pw::spi
