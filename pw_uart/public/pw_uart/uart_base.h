// Copyright 2024 The Pigweed Authors
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

#include "pw_status/status.h"

namespace pw::uart {

/// The common abstract base class of the UART interface.
///
/// The `UartBase` interface provides basic method for enabling and configuring
/// a UART. Methods for actually communicating via the UART are on the `Uart`
/// and `UartNonBlocking` child classes.

class UartBase {
 public:
  virtual ~UartBase() = default;

  /// Initializes the UART module, sets it into the default state as determined
  /// by the concrete UART implementation. This function should be a no-op if
  /// the UART module is in an enabled state.
  ///
  /// This may change the power state of the UART module, configure the
  /// interface parameters, enable the associated pins, setup the internal
  /// TX and RX buffers, etc...
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART module has been successfully initialized.
  ///
  ///    INTERNAL: Internal errors within the hardware abstraction layer.
  ///
  /// @endrst
  Status Enable() { return DoEnable(true); }

  /// Disables the UART module. Disabling the UART shuts down communication and
  /// prevents the microcontroller from sending or receiving data through the
  /// UART port.
  ///
  /// This is usually done to save power. Interrupt handlers should also be
  /// disabled.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART module has been successfully initialized.
  ///
  ///    INTERNAL: Internal errors  within the hardware abstraction layer.
  ///
  /// @endrst
  Status Disable() { return DoEnable(false); }

  /// Configures the UART communication baud rate.
  ///
  /// This function sets the communication speed (baud rate) for the UART.
  /// Whether the baud rate can be changed while the UART is enabled depends on
  /// the specific implementation.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART has been successfully initialized.
  ///
  ///    FAILED_PRECONDITION: The device is enabled and does not support
  ///    changing settings on the fly.
  ///
  ///    INTERNAL: Internal errors  within the hardware abstraction layer.
  ///
  /// @endrst
  Status SetBaudRate(uint32_t baud_rate) { return DoSetBaudRate(baud_rate); }

  /// Configures the UART hardware flow control enable.
  ///
  /// This function sets the hardware flow control enable for the UART.
  /// Whether the flow control setting rate can be changed while the UART is
  /// enabled depends on the specific implementation.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART has been successfully initialized.
  ///
  ///    FAILED_PRECONDITION: The device is enabled and does not support
  ///    changing settings on the fly.
  ///
  ///    UNIMPLEMENTED: The device does not support flow control.
  ///
  ///    INTERNAL: Internal errors within the hardware abstraction layer.
  ///
  /// @endrst
  Status SetFlowControl(bool enable) { return DoSetFlowControl(enable); }

 private:
  virtual Status DoEnable(bool enable) = 0;
  virtual Status DoSetBaudRate(uint32_t baud_rate) = 0;
  virtual Status DoSetFlowControl(bool /*enable*/) {
    return pw::Status::Unimplemented();
  }
};

}  // namespace pw::uart
