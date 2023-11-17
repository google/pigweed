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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_LE_CONNECTION_PARAMETERS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_LE_CONNECTION_PARAMETERS_H_

#include <cpp-string/string_printf.h>

#include <cstdint>
#include <string>

namespace bt::hci_spec {

// All LE connection parameters in this file are given in terms of a
// "multiplier" that to controller uses to calculate actual duration values.
// The exact value calculations and ranges depend on the context in which these
// values are used (e.g. see LECreateConnectionCommand).
//
// See the specification section referenced above each LEConnectionParameters
// getter below. See hci_constants.h for min/max allowed ranges for each
// parameter.

// Connection parameters for a LE connection. Used to represent parameters in
// active use by the link layer for a particular logical link.
class LEConnectionParameters final {
 public:
  LEConnectionParameters(uint16_t interval,
                         uint16_t latency,
                         uint16_t supervision_timeout);

  // Default constructor initializes all values to 0. This is intended for cases
  // that require default initialization, e.g. when this structure is used
  // inside a container that default initializes its contents.
  LEConnectionParameters();

  // The connection interval indicates the frequency of link layer connection
  // events over which data channel PDUs can be transmitted. See Core Spec v5.0,
  // Vol 6, Part B, Section 4.5.1 for more information on the link layer
  // connection events.
  uint16_t interval() const { return interval_; }

  // The maximum allowed connection latency. See Core Spec v5.0, Vol 6, Part
  // B, Section 4.5.2.
  uint16_t latency() const { return latency_; }

  // This defines the maximum time between two received data packet PDUs
  // before the connection is considered lost. See Core Spec v5.0, Vol 6, Part
  // B, Section 4.5.2. This value is in centiseconds and must be within
  // the range 100 ms - 32 s (10 cs - 3200 cs).
  uint16_t supervision_timeout() const { return supervision_timeout_; }

  // Equality operator
  bool operator==(const LEConnectionParameters& other) const;

  std::string ToString() const;

 private:
  uint16_t interval_;
  uint16_t latency_;
  uint16_t supervision_timeout_;
};

// Preferred connection parameters for LE connection. Used to represent
// parameters that act as a hint to the controller. This is used for:
//
//   * initiating a new connection as a central
//   * to represent a peripheral's preferred connection parameters
class LEPreferredConnectionParameters final {
 public:
  LEPreferredConnectionParameters(uint16_t min_interval,
                                  uint16_t max_interval,
                                  uint16_t max_latency,
                                  uint16_t supervision_timeout);

  // Default constructor initializes all values to 0. This is intended for cases
  // that require default initialization, e.g. when this structure is used
  // inside a container that default initializes its contents.
  LEPreferredConnectionParameters();

  // See hci_spec::LEConnectionParameters for a description of these fields. See
  // hci_constants.h for the allowed ranges for these values.
  uint16_t min_interval() const { return min_interval_; }
  uint16_t max_interval() const { return max_interval_; }
  uint16_t max_latency() const { return max_latency_; }
  uint16_t supervision_timeout() const { return supervision_timeout_; }

  // Equality operator
  bool operator==(const LEPreferredConnectionParameters& other) const;

 private:
  uint16_t min_interval_;
  uint16_t max_interval_;
  uint16_t max_latency_;
  uint16_t supervision_timeout_;
};

}  // namespace bt::hci_spec

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_LE_CONNECTION_PARAMETERS_H_
