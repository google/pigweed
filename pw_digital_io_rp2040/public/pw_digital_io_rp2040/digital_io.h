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

#include "pw_digital_io/digital_io.h"
#include "pw_digital_io/polarity.h"

namespace pw::digital_io {

struct Rp2040Config {
  uint16_t pin;
  Polarity polarity;

  bool operator==(const Rp2040Config& rhs) const {
    return polarity == rhs.polarity && pin == rhs.pin;
  }
  State PhysicalToLogical(const bool hal_value) const {
    return polarity == Polarity::kActiveHigh ? State(hal_value)
                                             : State(!hal_value);
  }
  bool LogicalToPhysical(const State state) const {
    return polarity == Polarity::kActiveHigh ? (bool)state : !(bool)state;
  }
};

class Rp2040DigitalInOut : public DigitalInOut {
 public:
  Rp2040DigitalInOut(Rp2040Config config);

 private:
  Status DoEnable(bool enable) override;
  Status DoSetState(State level) override;
  Result<State> DoGetState() override;

  Rp2040Config config_;
};

class Rp2040DigitalIn : public DigitalIn {
 public:
  Rp2040DigitalIn(Rp2040Config config);

 private:
  Status DoEnable(bool enable) override;
  Result<State> DoGetState() override;

  Rp2040Config config_;
};

}  // namespace pw::digital_io
