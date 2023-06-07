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

#include "fsl_gpio.h"
#include "pw_digital_io/digital_io.h"

namespace pw::digital_io {

class McuxpressoDigitalOut : public pw::digital_io::DigitalOut {
 public:
  McuxpressoDigitalOut(GPIO_Type* base,
                       uint32_t port,
                       uint32_t pin,
                       pw::digital_io::State initial_state);

  bool is_enabled() const { return enabled_; }

 private:
  pw::Status DoEnable(bool enable) override;
  pw::Status DoSetState(pw::digital_io::State state) override;

  GPIO_Type* base_;
  const uint32_t port_;
  const uint32_t pin_;
  const pw::digital_io::State initial_state_;
  bool enabled_ = false;
};

class McuxpressoDigitalIn : public pw::digital_io::DigitalIn {
 public:
  McuxpressoDigitalIn(GPIO_Type* base, uint32_t port, uint32_t pin);

  bool is_enabled() const { return enabled_; }

 private:
  pw::Status DoEnable(bool enable) override;
  pw::Result<pw::digital_io::State> DoGetState() override;

  GPIO_Type* base_;
  const uint32_t port_;
  const uint32_t pin_;
  bool enabled_ = false;
};

}  // namespace pw::digital_io
