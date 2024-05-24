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

static pw::Status SetSelector(uint32_t) {
  // Action to set selector
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

// DOCSTAG: [pw_clock_tree-examples-ClockSelectorExampleDef]
/// Class template implementing a clock selector.
///
/// Template argument `ElementType` can be of class `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or
/// `ElementNonBlockingMightFail.`
template <typename ElementType>
class ClockSelectorExample
    : public pw::clock_tree::DependentElement<ElementType> {
 public:
  constexpr ClockSelectorExample(ElementType& source,
                                 uint32_t selector,
                                 uint32_t selector_enable,
                                 uint32_t selector_disable)
      : pw::clock_tree::DependentElement<ElementType>(source),
        selector_(selector),
        selector_enable_(selector_enable),
        selector_disable_(selector_disable) {}

  pw::Status SetSource(ElementType& new_source, uint32_t new_selector_enable) {
    // Store a copy of the current `selector_enable_` variable in case
    // that the update fails, since we need to update `selector_enable_`
    // to its new value, since `UpdateSource` might call the `DoEnable`
    // member function.
    uint32_t old_selector_enable = selector_enable_;
    selector_enable_ = new_selector_enable;
    const bool kPermitChangeIfInUse = true;
    pw::Status status = this->UpdateSource(new_source, kPermitChangeIfInUse);
    if (!status.ok()) {
      // Restore the old selector value.
      selector_enable_ = old_selector_enable;
    }

    return status;
  }

 private:
  pw::Status DoEnable() final { return SetSelector(selector_enable_); }
  pw::Status DoDisable() final { return SetSelector(selector_disable_); }

  uint32_t selector_;
  uint32_t selector_enable_;
  uint32_t selector_disable_;
  template <typename U>
  friend class ClockTreeSetSource;
};
using ClockSelectorExampleNonBlocking =
    ClockSelectorExample<pw::clock_tree::ElementNonBlockingCannotFail>;
// DOCSTAG: [pw_clock_tree-examples-ClockSelectorExampleDef]

// DOCSTAG: [pw_clock_tree-examples-ClockTreeSetSourcesExampleDef]
/// Class implementing a clock tree that also supports the `UpdateSource`
/// method of the `ClockSelectorExample` class template.
class ClockTreeSetSourceExample : public pw::clock_tree::ClockTree {
 public:
  /// SetSource could be implemented for the other clock tree element classes
  /// as well.
  void SetSource(ClockSelectorExampleNonBlocking& element,
                 pw::clock_tree::ElementNonBlockingCannotFail& new_source,
                 uint32_t selector_enable) {
    std::lock_guard lock(interrupt_spin_lock_);
    element.SetSource(new_source, selector_enable).IgnoreError();
  }
};
// DOCSTAG: [pw_clock_tree-examples-ClockTreeSetSourcesExampleDef]

TEST(ClockTree, ClockTreeElementExample) {
  // DOCSTAG: [pw_clock_tree-examples-ClockTreeDec]
  // Create the clock tree
  ClockTreeSetSourceExample clock_tree;
  // DOCSTAG: [pw_clock_tree-examples-ClockTreeDec]

  // DOCSTAG: [pw_clock_tree-examples-ClockTreeElementsDec]
  // Define the clock tree
  ClockSourceExampleNonBlocking clock_a;
  ClockSourceExampleNonBlocking clock_b;

  const uint32_t kSelectorId = 7;
  const uint32_t kSelectorEnable1 = 2;
  const uint32_t kSelectorEnable2 = 4;
  const uint32_t kSelectorDisable = 7;
  // clock_selector_c depends on clock_a.
  ClockSelectorExampleNonBlocking clock_selector_c(
      clock_a, kSelectorId, kSelectorEnable1, kSelectorDisable);

  const uint32_t kDividerId = 12;
  const uint32_t kDividerValue1 = 42;
  // clock_divider_d depends on clock_b.
  ClockDividerExampleNonBlocking clock_divider_d(
      clock_b, kDividerId, kDividerValue1);
  // DOCSTAG: [pw_clock_tree-examples-ClockTreeElementsDec]

  // DOCSTAG: [pw_clock_tree-examples-AcquireClockSelectorC]
  // Acquire a reference to clock_selector_c, which will enable clock_selector_c
  // and its dependent clock_a.
  clock_tree.Acquire(clock_selector_c);
  // DOCSTAG: [pw_clock_tree-examples-AcquireClockSelectorC]

  // DOCSTAG: [pw_clock_tree-examples-ChangeClockSelectorCDependentSource]
  // Change clock_selector_c to depend on clock_divider_d.
  // This enables clock_b and clock_divider_d, and disables clock_a.
  clock_tree.SetSource(clock_selector_c, clock_divider_d, kSelectorEnable2);
  // DOCSTAG: [pw_clock_tree-examples-ChangeClockSelectorCDependentSource]

  // DOCSTAG: [pw_clock_tree-examples-SetClockDividerDValue]
  // Change the divider value for clock_divider_d.
  const uint32_t kDividerValue2 = 21;
  clock_tree.SetDividerValue(clock_divider_d, kDividerValue2);
  // DOCSTAG: [pw_clock_tree-examples-SetClockDividerDValue]

  // DOCSTAG: [pw_clock_tree-examples-ReleaseClockSelectorC]
  // Release reference to clock_selector_c, which will disable clock_selector_c,
  // clock_divider_d, and clock_b.
  clock_tree.Release(clock_selector_c);
  // All clock tree elements are disabled now.
  // DOCSTAG: [pw_clock_tree-examples-ReleaseClockSelectorC]
}

static pw::Status USART_RTOS_Init() { return pw::OkStatus(); }
static void USART_RTOS_Deinit() {}

// DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]
#include "pw_clock_tree/clock_tree.h"

class UartStreamMcuxpresso {
 public:
  pw::Status Init() {
    // Acquire reference to clock before initializing device.
    clock_tree_.Acquire(clock_tree_element_);
    pw::Status status = USART_RTOS_Init();
    if (!status.ok()) {
      // Failed to initialize device, release the acquired clock.
      clock_tree_.Release(clock_tree_element_);
    }
    return status;
  }

  void Deinit() {
    // Deinitialize the device before we can release the reference
    // to the clock.
    USART_RTOS_Deinit();
    clock_tree_.Release(clock_tree_element_);
  }

  // Device constructor that accepts `clock_tree` and `clock_tree_element`
  // to manage clock lifecycle.
  constexpr UartStreamMcuxpresso(
      pw::clock_tree::ClockTree& clock_tree,
      pw::clock_tree::ElementNonBlockingCannotFail& clock_tree_element)
      : clock_tree_(clock_tree), clock_tree_element_(clock_tree_element) {}

 private:
  pw::clock_tree::ClockTree& clock_tree_;
  pw::clock_tree::ElementNonBlockingCannotFail& clock_tree_element_;
};
// DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]

using ClockSourceUart =
    ClockSourceExample<pw::clock_tree::ElementNonBlockingCannotFail>;

pw::Status ClockTreeExample() {
  // DOCSTAG: [pw_clock_tree-examples-IntegrationIntoDeviceDriversUsage]

  // Declare the clock tree
  pw::clock_tree::ClockTree clock_tree;
  // Declare the uart clock source
  ClockSourceUart uart_clock_source;
  UartStreamMcuxpresso uart(clock_tree, uart_clock_source);

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
