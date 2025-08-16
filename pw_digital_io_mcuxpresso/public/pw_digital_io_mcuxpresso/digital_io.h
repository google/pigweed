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
#include "pw_containers/intrusive_forward_list.h"
#include "pw_digital_io/digital_io.h"

namespace pw::digital_io {

/// @module{pw_digital_io_mcuxpresso}

PW_EXTERN_C void GPIO_INTA_DriverIRQHandler();

/// Provides output-only support for an MCUXpresso GPIO pin.
///
/// Class-specific behaviors:
/// * When `Disable` is called, the GPIO is configured as an input, which
///   disables the output driver.
class McuxpressoDigitalOut : public pw::digital_io::DigitalOut {
 public:
  /// Constructs a McuxpressoDigitalOut for a specific GPIO module+port+pin.
  ///
  /// @param[in] base The base address of the GPIO module (e.g. `GPIO`).
  /// @param[in] port The port number on the given GPIO module.
  /// @param[in] pin The pin number on the given GPIO port.
  /// @param[in] initial_state The state to be driven when the line is enabled.
  McuxpressoDigitalOut(GPIO_Type* base,
                       uint32_t port,
                       uint32_t pin,
                       pw::digital_io::State initial_state);

  /// Returns true if the output is enabled.
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

/// Provides input-only support for an MCUXpresso GPIO pin.
///
/// Class-specific behaviors:
/// * The input buffer for the pin must be enabled in the IO Pad Controller
///   (`IOPCTL`) via the Input Buffer Enable (`IBENA`) bit.
/// * The input polarity is affected by the Input Invert Enable (`IIENA`) bit
///   on the corresponding IO Pad Controller (`IOPCTL`) register.
class McuxpressoDigitalIn : public pw::digital_io::DigitalIn {
 public:
  /// Constructs a McuxpressoDigitalIn for a specific GPIO module+port+pin.
  ///
  /// @param[in] base The base address of the GPIO module (e.g. `GPIO`).
  /// @param[in] port The port number on the given GPIO module.
  /// @param[in] pin The pin number on the given GPIO port.
  McuxpressoDigitalIn(GPIO_Type* base, uint32_t port, uint32_t pin);

  /// Returns true if the input is enabled.
  bool is_enabled() const { return enabled_; }

 private:
  pw::Status DoEnable(bool enable) override;
  pw::Result<pw::digital_io::State> DoGetState() override;

  GPIO_Type* base_;
  const uint32_t port_;
  const uint32_t pin_;
  bool enabled_ = false;
};

/// Provides input, output, and interrupt support for an MCUXpresso GPIO pin.
///
/// Interrupts are provided by IRQ "A" on the GPIO module.
///
/// Class-specific behaviors:
/// * The direction of the pin cannot be changed after construction.
/// * If configured as an output:
///   * The default state on `Enable` is 0 (inactive).
///   * `Disable` has no actual effect; unlike McuxpressoDigitalOut, the GPIO
///     is not reverted to an input.
/// * If configured as an input:
///   * The input buffer for the pin must be enabled in the IO Pad Controller
///     (`IOPCTL`) via the Input Buffer Enable (`IBENA`) bit.
///   * The input polarity is affected by the Input Invert Enable (`IIENA`) bit
///     on the corresponding IO Pad Controller (`IOPCTL`) register.
class McuxpressoDigitalInOutInterrupt
    : public pw::digital_io::DigitalInOutInterrupt,
      public pw::IntrusiveForwardList<McuxpressoDigitalInOutInterrupt>::Item {
 public:
  /// Constructs a McuxpressoDigitalInOutInterrupt for a specific GPIO
  /// module+port+pin.
  ///
  /// @param[in] base The base address of the GPIO module (e.g. `GPIO`).
  ///
  /// @param[in] port The port number on the given GPIO module.
  ///
  /// @param[in] pin The pin number on the given GPIO port.
  ///
  /// @param[in] output True if the line should be configured as an output on
  /// enable; False if it should be an input.
  McuxpressoDigitalInOutInterrupt(GPIO_Type* base,
                                  uint32_t port,
                                  uint32_t pin,
                                  bool output);

  /// Returns true if the line is enabled (in any state).
  bool is_enabled() const { return enabled_; }

 private:
  friend void GPIO_INTA_DriverIRQHandler();
  pw::Status DoEnable(bool enable) override;
  pw::Result<pw::digital_io::State> DoGetState() override;
  pw::Status DoSetState(pw::digital_io::State state) override;
  pw::Status DoSetInterruptHandler(
      pw::digital_io::InterruptTrigger trigger,
      pw::digital_io::InterruptHandler&& handler) override;
  pw::Status DoEnableInterruptHandler(bool enable) override;

  void ConfigureInterrupt() const;

  GPIO_Type* base_;
  const uint32_t port_;
  const uint32_t pin_;
  const bool output_;
  pw::digital_io::InterruptTrigger trigger_;
  pw::digital_io::InterruptHandler interrupt_handler_;
  bool enabled_ = false;
};

}  // namespace pw::digital_io
