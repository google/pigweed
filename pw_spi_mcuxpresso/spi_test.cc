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

#include "pw_spi_mcuxpresso/spi.h"

#include "board.h"
#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_spi/device.h"
#include "pw_status/status.h"

namespace pw::spi {
namespace {

auto* spi_base = SPI14;
constexpr auto kClockNumber = 14;
constexpr uint32_t baud_rate_bps = 10000000;
constexpr Config configuration{.polarity = ClockPolarity::kActiveHigh,
                               .phase = ClockPhase::kRisingEdge,
                               .bits_per_word = BitsPerWord(8),
                               .bit_order = BitOrder::kMsbFirst};

TEST(Configure, ConfigurationSuccess) {
  McuxpressoInitiator spi(
      spi_base, CLOCK_GetFlexcommClkFreq(kClockNumber), baud_rate_bps);
  auto status = spi.Configure(configuration);

  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, PollingWriteSuccess) {
  const auto blocking = true;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  static auto destination = bytes::Array<0xff, 0xff, 0xff, 0xff, 0xff>();

  McuxpressoInitiator spi(spi_base,
                          CLOCK_GetFlexcommClkFreq(kClockNumber),
                          baud_rate_bps,
                          blocking);
  auto status = spi.Configure(configuration);
  ASSERT_EQ(status, OkStatus());

  status = spi.WriteRead(source, destination);
  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, IRQWriteSuccess) {
  const auto blocking = false;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  static auto destination = bytes::Array<0xff, 0xff, 0xff, 0xff, 0xff>();
  McuxpressoInitiator spi(spi_base,
                          CLOCK_GetFlexcommClkFreq(kClockNumber),
                          baud_rate_bps,
                          blocking);
  auto status = spi.Configure(configuration);
  ASSERT_EQ(status, OkStatus());

  status = spi.WriteRead(source, destination);
  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, WriteOnlySuccess) {
  const auto blocking = false;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  McuxpressoInitiator spi(spi_base,
                          CLOCK_GetFlexcommClkFreq(kClockNumber),
                          baud_rate_bps,
                          blocking);
  auto status = spi.Configure(configuration);
  ASSERT_EQ(status, OkStatus());

  status = spi.WriteRead(source, {});
  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, UseDeviceWriteOnlySuccess) {
  const auto blocking = false;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  McuxpressoInitiator initiator(spi_base,
                                CLOCK_GetFlexcommClkFreq(kClockNumber),
                                baud_rate_bps,
                                blocking);
  McuxpressoChipSelector spi_selector(initiator, 0);
  sync::VirtualMutex spi_lock;
  sync::Borrowable<Initiator> spi(initiator, spi_lock);
  Device device(spi, configuration, spi_selector);

  EXPECT_EQ(device.WriteRead(source, {}), OkStatus());
}
}  // namespace
}  // namespace pw::spi
