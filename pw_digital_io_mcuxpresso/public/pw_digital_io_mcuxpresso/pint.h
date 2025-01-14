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
#include "pw_status/status.h"
#include "pw_sync/borrow.h"

namespace pw::digital_io {

/// Abstracts the Pin Interrupt (PINT) module.
///
/// One instance of this class should be created to enable the creation of
/// `McuxpressoPintInterrupt` lines.
class McuxpressoPintController {
 public:
  /// Constructs a McuxpressoPintController for an instance of the PINT module.
  ///
  /// @param[in] base The base address of the PINT module (e.g. `PINT`).
  McuxpressoPintController(PINT_Type* base);
  ~McuxpressoPintController();

  McuxpressoPintController(const McuxpressoPintController&) = delete;
  McuxpressoPintController& operator=(const McuxpressoPintController&) = delete;

 private:
  friend class McuxpressoPintInterrupt;

  pw::Status Config(pint_pin_int_t pin,
                    pw::digital_io::InterruptTrigger trigger,
                    pw::digital_io::InterruptHandler&& handler);
  pw::Status EnableHandler(pint_pin_int_t pin, bool enable);

  PINT_Type* base_;
};

// Deprecated alias.
using McuxpressoInterruptController = McuxpressoPintController;

/// Represents one interrupt on the PINT module.
///
/// Class-specific behaviors:
/// * The pin must be attached to the PINT module via
///   `INPUTMUX_AttachSignal()`.
/// * `Enable` and `Disable` have no effect.
/// * The input buffer for the pin must be enabled in the IO Pad Controller
///   (`IOPCTL`) via the Input Buffer Enable (`IBENA`) bit.
/// * The input polarity is affected by the Input Invert Enable (`IIENA`) bit
///   on the corresponding IO Pad Controller (`IOPCTL`) register.
class McuxpressoPintInterrupt : public pw::digital_io::DigitalInterrupt {
 public:
  /// Constructs a McuxpressoPintInterrupt for a specific pin.
  ///
  /// @param[in] controller A `pw::sync::Borrowable` reference to the
  /// `McuxpressoPintInterrupt` representing the PINT module.
  ///
  /// @param[in] pin The `pint_pin_int_t` enum member identifying the pin
  /// interrupt on the PINT module.
  McuxpressoPintInterrupt(
      pw::sync::Borrowable<McuxpressoPintController>& controller,
      pint_pin_int_t pin);

  McuxpressoPintInterrupt(const McuxpressoPintInterrupt&) = delete;
  McuxpressoPintInterrupt& operator=(const McuxpressoPintInterrupt&) = delete;

 private:
  // pw::digital_io::DigitalInterrupt implementation
  pw::Status DoEnable(bool enable) override;
  pw::Status DoSetInterruptHandler(
      pw::digital_io::InterruptTrigger trigger,
      pw::digital_io::InterruptHandler&& handler) override;
  pw::Status DoEnableInterruptHandler(bool enable) override;

  pw::sync::Borrowable<McuxpressoPintController>& controller_;
  pint_pin_int_t pin_;
};

}  // namespace pw::digital_io
