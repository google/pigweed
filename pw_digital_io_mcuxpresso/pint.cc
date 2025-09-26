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

// PINT_USE_LEGACY_CALLBACK is defined as 0 or 1 when using SDK 25.06.00 and
// above, and not defined at at in older versions.
#if defined(PINT_USE_LEGACY_CALLBACK)
void PintCallback(pint_pin_int_t pin, pint_status_t*) {
#else
void PintCallback(pint_pin_int_t pin, uint32_t) {
#endif  // defined(PINT_USE_LEGACY_CALLBACK)
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

// Using SDK 25.06.00 without the legacy callback API.
#if defined(PINT_USE_LEGACY_CALLBACK) && !PINT_USE_LEGACY_CALLBACK
  PINT_SetCallback(base_, PintCallback);
#endif  // defined(PINT_USE_LEGACY_CALLBACK) && !PINT_USE_LEGACY_CALLBACK
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
  pint_pin_enable_t enable = kPINT_PinIntEnableNone;
  switch (trigger) {
    case InterruptTrigger::kActivatingEdge:
      enable = kPINT_PinIntEnableRiseEdge;
      break;
    case InterruptTrigger::kDeactivatingEdge:
      enable = kPINT_PinIntEnableFallEdge;
      break;
    case InterruptTrigger::kBothEdges:
      enable = kPINT_PinIntEnableBothEdges;
      break;
    default:
      return pw::Status::InvalidArgument();
  }

#if defined(PINT_USE_LEGACY_CALLBACK) && !PINT_USE_LEGACY_CALLBACK
  PINT_PinInterruptConfig(base_, pin, enable);
#else
  PINT_PinInterruptConfig(base_, pin, enable, PintCallback);
#endif  // defined(PINT_USE_LEGACY_CALLBACK) && !PINT_USE_LEGACY_CALLBACK

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
