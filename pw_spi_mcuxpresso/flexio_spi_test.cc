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

#include "pw_spi_mcuxpresso/flexio_spi.h"

#include "board.h"
#include "pw_bytes/array.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::spi {
namespace {

FLEXIO_SPI_Type flexio_spi_config = {
    .flexioBase = reinterpret_cast<FLEXIO_Type*>(FLEXIO0),
    .SDOPinIndex = 13,
    .SDIPinIndex = 14,
    .SCKPinIndex = 15,
    .CSnPinIndex = 12,
    .shifterIndex = {0, 2},
    .timerIndex = {0, 1}};
constexpr uint32_t baud_rate_bps = 500000;
constexpr Config configuration{.polarity = ClockPolarity::kActiveLow,
                               .phase = ClockPhase::kFallingEdge,
                               .bits_per_word = BitsPerWord(8),
                               .bit_order = BitOrder::kMsbFirst};

TEST(Configure, ConfigurationSuccess) {
  McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps);
  auto status = spi.Configure(configuration);

  EXPECT_EQ(status, OkStatus());
}

TEST(Configure, RepeatedConfigurationSuccess) {
  McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps);
  auto status = spi.Configure(configuration);
  EXPECT_EQ(status, OkStatus());

  status = spi.Configure(configuration);
  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, PollingWriteSuccess) {
  const auto blocking = true;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  static auto destination = bytes::Array<0xff, 0xff, 0xff, 0xff, 0xff>();

  McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps, blocking);
  auto status = spi.Configure(configuration);
  ASSERT_EQ(status, OkStatus());

  status = spi.WriteRead(source, destination);
  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, IRQWriteSuccess) {
  const auto blocking = false;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  static auto destination = bytes::Array<0xff, 0xff, 0xff, 0xff, 0xff>();
  McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps, blocking);
  auto status = spi.Configure(configuration);
  ASSERT_EQ(status, OkStatus());

  status = spi.WriteRead(source, destination);
  EXPECT_EQ(status, OkStatus());
}

TEST(ReadWrite, WriteOnlySuccess) {
  const auto blocking = false;
  constexpr auto source = bytes::Array<0x01, 0x02, 0x03, 0x04, 0x05>();
  McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps, blocking);
  auto status = spi.Configure(configuration);
  ASSERT_EQ(status, OkStatus());

  status = spi.WriteRead(source, {});
  EXPECT_EQ(status, OkStatus());
}

}  // namespace
}  // namespace pw::spi
