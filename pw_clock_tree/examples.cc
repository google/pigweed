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

#include "pw_assert/check.h"
#include "pw_clock_tree/clock_tree.h"
#include "pw_unit_test/framework.h"

namespace examples {

static pw::Status EnableClock() {
  // Action to enable clock
  return pw::OkStatus();
}

static pw::Status DisableClock() {
  // Action to disable clock
  return pw::OkStatus();
}

static pw::Status EnableClockDivider(uint32_t, uint32_t) {
  // Action to enable clock divider
  return pw::OkStatus();
}

// DOCSTAG: [pw_clock_tree-examples-ClockSourceExampleDef]
/// Class template implementing a clock source.
///
/// Template argument `ElementType` can be of class `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or
/// `ElementNonBlockingMightFail.`
template <typename ElementType>
class ClockSourceExample : public pw::clock_tree::ClockSource<ElementType> {
 private:
  pw::Status DoEnable() final { return EnableClock(); }

  pw::Status DoDisable() final { return DisableClock(); }
};
using ClockSourceExampleNonBlocking =
    ClockSourceExample<pw::clock_tree::ElementNonBlockingCannotFail>;
// DOCSTAG: [pw_clock_tree-examples-ClockSourceExampleDef]

// DOCSTAG: [pw_clock_tree-examples-ClockDividerExampleDef]
/// Class template implementing a clock divider.
///
/// Template argument `ElementType` can be of class `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or
/// `ElementNonBlockingMightFail.`
template <typename ElementType>
class ClockDividerExample
    : public pw::clock_tree::ClockDividerElement<ElementType> {
 public:
  constexpr ClockDividerExample(ElementType& source,
                                uint32_t divider_name,
                                uint32_t divider)
      : pw::clock_tree::ClockDividerElement<ElementType>(source, divider),
        divider_name_(divider_name) {}

 private:
  pw::Status DoEnable() final {
    return EnableClockDivider(divider_name_, this->divider());
  }

  uint32_t divider_name_;
};
using ClockDividerExampleNonBlocking =
    ClockDividerExample<pw::clock_tree::ElementNonBlockingCannotFail>;
// DOCSTAG: [pw_clock_tree-examples-ClockDividerExampleDef]

TEST(ClockTree, ClockTreeElementExample) {
  // DOCSTAG: [pw_clock_tree-examples-ClockTreeElementsDec]
  // Define the clock tree
  ClockSourceExampleNonBlocking clock_a;

  const uint32_t kDividerId = 12;
  const uint32_t kDividerValue1 = 42;
  // clock_divider_d depends on clock_a.
  ClockDividerExampleNonBlocking clock_divider_d(
      clock_a, kDividerId, kDividerValue1);
  // DOCSTAG: [pw_clock_tree-examples-ClockTreeElementsDec]

  // DOCSTAG: [pw_clock_tree-examples-AcquireClockDividerD]
  // Acquire a reference to clock_divider_d, which will enable clock_divider_d
  // and its dependent clock_a.
  clock_divider_d.Acquire();
  // DOCSTAG: [pw_clock_tree-examples-AcquireClockDividerD]

  // DOCSTAG: [pw_clock_tree-examples-SetClockDividerDValue]
  // Change the divider value for clock_divider_d.
  const uint32_t kDividerValue2 = 21;
  clock_divider_d.SetDivider(kDividerValue2);
  // DOCSTAG: [pw_clock_tree-examples-SetClockDividerDValue]

  // DOCSTAG: [pw_clock_tree-examples-ReleaseClockSelectorC]
  // Release reference to clock_divider_d, which will disable clock_divider_d,
  // and clock_a.
  clock_divider_d.Release();
  // All clock tree elements are disabled now.
  // DOCSTAG: [pw_clock_tree-examples-ReleaseClockSelectorC]
}

static pw::Status USART_RTOS_Init() { return pw::OkStatus(); }
static void USART_RTOS_Deinit() {}

// DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]
#include "pw_clock_tree/clock_tree.h"

class UartStreamMcuxpresso {
 public:
  // Device constructor that optionally accepts `clock_tree_element` to manage
  // clock lifecycle.
  constexpr UartStreamMcuxpresso(pw::clock_tree::ElementNonBlockingCannotFail*
                                     clock_tree_element = nullptr)
      : clock_tree_element_(clock_tree_element) {}

  pw::Status Init() {
    // Acquire reference to clock before initializing device.
    PW_TRY(clock_tree_element_.Acquire());
    pw::Status status = USART_RTOS_Init();
    if (!status.ok()) {
      // Failed to initialize device, release the acquired clock.
      clock_tree_element_.Release().IgnoreError();
    }
    return status;
  }

  void Deinit() {
    // Deinitialize the device before we can release the reference
    // to the clock.
    USART_RTOS_Deinit();
    clock_tree_element_.Release().IgnoreError();
  }

 private:
  pw::clock_tree::OptionalElement clock_tree_element_;
};
// DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]

using ClockSourceUart =
    ClockSourceExample<pw::clock_tree::ElementNonBlockingCannotFail>;

pw::Status ClockTreeExample() {
  // DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversUsage]

  // Declare the uart clock source
  ClockSourceUart uart_clock_source;
  UartStreamMcuxpresso uart(&uart_clock_source);

  // Initialize the uart which enables the uart clock source.
  PW_TRY(uart.Init());
  PW_CHECK(uart_clock_source.ref_count() > 0);

  // Do something with uart

  // Deinitialize the uart which disable the uart clock source.
  uart.Deinit();

  // DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversUsage]

  return pw::OkStatus();
}

TEST(ClockTree, ClockTreeExample) {
  pw::Status status = ClockTreeExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}
}  // namespace examples
