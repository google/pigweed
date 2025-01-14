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

#include "pw_digital_io_mcuxpresso/pint.h"

#include <array>
#include <cstdint>

#include "fsl_pint.h"
#include "pw_function/function.h"

namespace pw::digital_io {
namespace {

using ::pw::digital_io::InterruptTrigger;
using ::pw::digital_io::State;

// PINT API doesn't allow context on callback API, so store globally.
std::array<pw::digital_io::InterruptHandler,
           FSL_FEATURE_PINT_NUMBER_OF_CONNECTED_OUTPUTS>
    interrupt_handlers;
std::array<PINT_Type*, FSL_FEATURE_PINT_NUMBER_OF_CONNECTED_OUTPUTS> bases;

void PintCallback(pint_pin_int_t pin, uint32_t) {
  PW_CHECK(pin < interrupt_handlers.size());
  State state = PINT_PinInterruptGetStatus(bases[pin], pin) == 1
                    ? State::kActive
                    : State::kInactive;
  interrupt_handlers[pin](state);
  SDK_ISR_EXIT_BARRIER;
}

}  // namespace

// McuxpressoPintController

McuxpressoPintController::McuxpressoPintController(PINT_Type* base)
    : base_(base) {
  PINT_Init(base_);
}

McuxpressoPintController::~McuxpressoPintController() { PINT_Deinit(base_); }

pw::Status McuxpressoPintController::Config(
    pint_pin_int_t pin,
    InterruptTrigger trigger,
    pw::digital_io::InterruptHandler&& handler) {
  if (pin >= interrupt_handlers.size()) {
    return pw::Status::InvalidArgument();
  }
  interrupt_handlers[pin] = std::move(handler);
  bases[pin] = base_;
  switch (trigger) {
    case InterruptTrigger::kActivatingEdge:
      PINT_PinInterruptConfig(
          base_, pin, kPINT_PinIntEnableRiseEdge, PintCallback);
      break;
    case InterruptTrigger::kDeactivatingEdge:
      PINT_PinInterruptConfig(
          base_, pin, kPINT_PinIntEnableFallEdge, PintCallback);
      break;
    case InterruptTrigger::kBothEdges:
      PINT_PinInterruptConfig(
          base_, pin, kPINT_PinIntEnableBothEdges, PintCallback);
      break;
    default:
      return pw::Status::InvalidArgument();
  }
  return pw::OkStatus();
}

pw::Status McuxpressoPintController::EnableHandler(pint_pin_int_t pin,
                                                   bool enable) {
  if (enable) {
    PINT_EnableCallbackByIndex(base_, pin);
  } else {
    PINT_DisableCallbackByIndex(base_, pin);
  }
  return pw::OkStatus();
}

// McuxpressoPintInterrupt

McuxpressoPintInterrupt::McuxpressoPintInterrupt(
    pw::sync::Borrowable<McuxpressoPintController>& controller,
    pint_pin_int_t pin)
    : controller_(controller), pin_(pin) {}

pw::Status McuxpressoPintInterrupt::DoEnable(bool) {
  // Can not enabled at individual line level. Only at controller level, which
  // is always enabled.
  return pw::OkStatus();
}

pw::Status McuxpressoPintInterrupt::DoSetInterruptHandler(
    pw::digital_io::InterruptTrigger trigger,
    pw::digital_io::InterruptHandler&& handler) {
  return controller_.acquire()->Config(pin_, trigger, std::move(handler));
}

pw::Status McuxpressoPintInterrupt::DoEnableInterruptHandler(bool enable) {
  return controller_.acquire()->EnableHandler(pin_, enable);
}

}  // namespace pw::digital_io
