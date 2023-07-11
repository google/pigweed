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

#include "fsl_pint.h"
#include "pw_digital_io/digital_io.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::digital_io {

class McuxpressoInterruptController {
 public:
  McuxpressoInterruptController(PINT_Type* base);
  ~McuxpressoInterruptController();

  McuxpressoInterruptController(const McuxpressoInterruptController&) = delete;
  McuxpressoInterruptController& operator=(
      const McuxpressoInterruptController&) = delete;

  pw::Status Config(pint_pin_int_t pin,
                    pw::digital_io::InterruptTrigger trigger,
                    pw::digital_io::InterruptHandler&& handler);
  pw::Status EnableHandler(pint_pin_int_t pin, bool enable);
  pw::Result<pw::digital_io::State> GetState(pint_pin_int_t pin);

 private:
  PINT_Type* base_;
};

}  // namespace pw::digital_io
