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

#include <cstdint>

#include "hardware/spi.h"
#include "pw_spi/initiator.h"

namespace pw::spi {

// Pico SDK userspace implementation of the SPI Initiator
class Rp2040Initiator : public Initiator {
 public:
  Rp2040Initiator(spi_inst_t* spi);

  // Implements pw::spi::Initiator:
  Status Configure(const Config& config) override;
  Status WriteRead(ConstByteSpan write_buffer, ByteSpan read_buffer) override;

 private:
  Status LazyInit();

  spi_inst_t* spi_;
  Status init_status_;         // The saved LazyInit() status.
  BitsPerWord bits_per_word_;  // Last Configure() bits_per_word.
};

}  // namespace pw::spi
