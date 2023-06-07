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

#include "pw_digital_io_mcuxpresso/interrupt_line.h"

#include "pw_digital_io/digital_io.h"
#include "pw_digital_io_mcuxpresso/interrupt_controller.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/borrow.h"

namespace pw::digital_io {

McuxpressoDigitalInInterrupt::McuxpressoDigitalInInterrupt(
    pw::sync::Borrowable<McuxpressoInterruptController>& controller,
    pint_pin_int_t pin)
    : controller_(controller), pin_(pin) {}

pw::Status McuxpressoDigitalInInterrupt::DoEnable(bool enable) {
  return controller_.acquire()->Enable(pin_);
}

pw::Result<pw::digital_io::State> McuxpressoDigitalInInterrupt::DoGetState() {
  return controller_.acquire()->GetState(pin_);
}

pw::Status McuxpressoDigitalInInterrupt::DoSetInterruptHandler(
    pw::digital_io::InterruptTrigger trigger,
    pw::digital_io::InterruptHandler&& handler) {
  return controller_.acquire()->Config(pin_, trigger, std::move(handler));
}

pw::Status McuxpressoDigitalInInterrupt::DoEnableInterruptHandler(bool enable) {
  return controller_.acquire()->EnableHandler(pin_, enable);
}

}  // namespace pw::digital_io
