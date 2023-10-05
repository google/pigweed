// Copyright 2022 The Pigweed Authors
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

namespace pw::digital_io {

class Rp2040DigitalInOut : public DigitalInOut {
 public:
  Rp2040DigitalInOut(uint32_t pin);

  Status DoEnable(bool enable) override;
  Status DoSetState(State level) override;
  Result<State> DoGetState() override;

 private:
  uint32_t pin_;
};

class Rp2040DigitalIn : public DigitalIn {
 public:
  Rp2040DigitalIn(uint32_t pin);

  Status DoEnable(bool enable) override;
  Result<State> DoGetState() override;

 private:
  uint32_t pin_;
};

}  // namespace pw::digital_io
