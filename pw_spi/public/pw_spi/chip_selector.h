// Copyright 2021 The Pigweed Authors
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

namespace pw::spi {

/// Configuration data used to determine how the Chip Select signal is
/// controlled throughout a transaction.
///
/// `kPerWriteRead` indicates that the chip-select signal should be
/// activated/deactivated between calls to WriteRead()
enum class ChipSelectBehavior : uint8_t {
  kPerWriteRead,
  kPerTransaction,
};

/// The ChipSelector class provides an abstract interface for controlling the
/// chip-select signal associated with a specific SPI responder.
///
/// This interface provides a `SetActive()` method, which activates/deactivates
/// the device based on the value of the `active` parameter.  The associated
/// `Activate()` and `Deactivate()` methods are utility wrappers for
/// `SetActive(true)` and `SetActive(false)`, respectively.
///
/// A concrete implementation of this interface class must be provided in order
/// to use the SPI HAL to communicate with a responder.
///
/// @note `Active` does not imply a specific logic-level; it is left to the
/// implementor to correctly map logic-levels to the device's active/inactive
/// states.
class ChipSelector {
 public:
  virtual ~ChipSelector() = default;

  /// SetActive sets the state of the chip-select signal to the value
  /// represented by the `active` parameter.  Passing a value of `true` will
  /// activate the chip-select signal, and `false` will deactivate the
  /// chip-select signal.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Success.
  ///
  /// Returns other implementation-specific values on failure.
  ///
  /// @endrst
  virtual Status SetActive(bool active) = 0;

  /// Helper method to activate the chip-select signal.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Success.
  ///
  /// Returns other implementation-specific values on failure.
  ///
  /// @endrst
  Status Activate() { return SetActive(true); }

  /// Helper method to deactivate the chip-select signal.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Success.
  ///
  /// Returns other implementation-specific values on failure.
  ///
  /// @endrst
  Status Deactivate() { return SetActive(false); }
};

}  // namespace pw::spi
