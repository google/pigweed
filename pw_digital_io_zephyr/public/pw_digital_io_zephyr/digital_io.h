// Copyright 2025 The Pigweed Authors
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

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <cstdint>

#include "pw_digital_io/digital_io.h"

/// @brief Callback handler used by all pw::digital_io::ZephyrDigital*Interrupt
///
/// @param dev Zephyr device pointer of the interrupt controller
/// @param cb Callback data structure registered with Zephyr
/// @param pins The pins that triggered the callback
extern "C" void pw_digital_io_ZephyrCallbackHandler(const struct device* dev,
                                                    struct gpio_callback* cb,
                                                    uint32_t pins);

namespace pw::digital_io {

/// @brief Convert Pigweed InterruptTrigger to Zephyr gpio flags
///
/// @param trigger The Pigweed trigger to convert
/// @return Corresponding Zephyr gpio_flags_t
constexpr gpio_flags_t InterruptTriggerToZephyrFlags(InterruptTrigger trigger) {
  switch (trigger) {
    case InterruptTrigger::kActivatingEdge:
      return GPIO_INT_EDGE_TO_ACTIVE;
    case InterruptTrigger::kDeactivatingEdge:
      return GPIO_INT_EDGE_TO_INACTIVE;
    case InterruptTrigger::kBothEdges:
      return GPIO_INT_EDGE_BOTH;
  }

  PW_UNREACHABLE;
}

/// @brief Wrapper for Zephyr callbacks
///
/// Used to map the Zephyr interrupt callback function to the Pigweed
/// InterruptHandler
struct gpio_callback_and_handler {
  struct gpio_callback data;  //< Zephyr gpio_callback data structure
  InterruptHandler handler;   //< Pigweed InterruptHandler to call
};

/// @brief Construct a bridge between Zephyr's gpio API and Pigweed's
///
/// This is a catch all class that will be used to implement the various Pigweed
/// Digital* classes.
///
/// @tparam kFlags Extra Zephyr flags used to initialize the GPIO
/// @tparam kUseInterrupts True to enable interrupt support
template <gpio_flags_t kFlags, bool kUseInterrupts>
class GenericZephyrDigitalInOut {
 public:
  /// @brief Construct a generic Digital I/O around a Zephyr devicetree spec
  ///
  /// @param dt_spec The spec to use, usually from DPIO_DT_SPEC_GET()
  constexpr GenericZephyrDigitalInOut(const struct gpio_dt_spec dt_spec)
      : gpio_spec_(dt_spec) {
    if (kUseInterrupts) {
      // We're using interrupts, init the callback object
      gpio_init_callback(&callback_.data,
                         pw_digital_io_ZephyrCallbackHandler,
                         BIT(gpio_spec_.pin));
    }
  }

 protected:
  Status DoEnable(bool enable) {
    // Select the right flags based on 'enable'
    gpio_flags_t flags = enable ? kFlags : GPIO_DISCONNECTED;

    // Configure the pin
    return gpio_pin_configure_dt(&gpio_spec_, flags) == 0
               ? pw::OkStatus()
               : pw::Status::Internal();
  }

  // Only enable if kFlags include GPIO_INPUT
  template <typename = std::enable_if<(kFlags & GPIO_INPUT) == GPIO_INPUT>>
  Result<State> DoGetState() {
    // Verify the device is ready
    int rc = gpio_is_ready_dt(&gpio_spec_);
    if (rc == 0) {
      return pw::Status::Unavailable();
    }
    // Get the current pin state
    rc = gpio_pin_get_dt(&gpio_spec_);
    return Result<State>(rc == 0 ? State::kInactive : State::kActive);
  }

  // Only enable if kFlags include GPIO_OUTPUT
  template <typename = std::enable_if<(kFlags & GPIO_OUTPUT) == GPIO_OUTPUT>>
  Status DoSetState(State state) {
    // Verify the device is ready
    int rc = gpio_is_ready_dt(&gpio_spec_);
    if (rc == 0) {
      return pw::Status::Unavailable();
    }

    // Set the pin state
    rc = gpio_pin_set_dt(&gpio_spec_, state == State::kActive ? 1 : 0);
    return rc == 0 ? pw::OkStatus() : pw::Status::Internal();
  }

  // Only enable if kUseInterrupts is true
  template <typename = std::enable_if<kUseInterrupts>>
  Status DoSetInterruptHandler(InterruptTrigger trigger,
                               InterruptHandler&& handler) {
    // Verify the device is ready
    int rc = gpio_is_ready_dt(&gpio_spec_);
    if (rc == 0) {
      return pw::Status::Unavailable();
    }

    // Configure the interrupt trigger
    rc = gpio_pin_interrupt_configure_dt(
        &gpio_spec_, InterruptTriggerToZephyrFlags(trigger));
    if (rc != 0) {
      return pw::Status::Internal();
    }

    // Save the handler
    callback_.handler = std::move(handler);
    return pw::OkStatus();
  }

  // Only enable if kUseInterrupts is true
  template <typename = std::enable_if<kUseInterrupts>>
  Status DoEnableInterruptHandler(bool enable) {
    if (!enable) {
      // Remove the callbacks to disable
      return gpio_remove_callback_dt(&gpio_spec_, &callback_.data) == 0
                 ? pw::OkStatus()
                 : pw::Status::Internal();
    }
    // If we don't have a handler, we can't enable
    if (callback_.handler == nullptr) {
      return pw::Status::FailedPrecondition();
    }
    // Add the callbacks to enable
    return gpio_add_callback_dt(&gpio_spec_, &callback_.data) == 0
               ? pw::OkStatus()
               : pw::Status::Internal();
  }

 private:
  const struct gpio_dt_spec gpio_spec_;
  struct gpio_callback_and_handler callback_;
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalIn
class ZephyrDigitalIn : public DigitalIn,
                        public GenericZephyrDigitalInOut<GPIO_INPUT, false> {
 public:
  using parent_type = GenericZephyrDigitalInOut<GPIO_INPUT, false>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Result<State> DoGetState() override { return parent_type::DoGetState(); }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalInInterrupt
class ZephyrDigitalInInterrupt
    : public DigitalInInterrupt,
      public GenericZephyrDigitalInOut<GPIO_INPUT, true> {
 public:
  using parent_type = GenericZephyrDigitalInOut<GPIO_INPUT, true>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Result<State> DoGetState() override { return parent_type::DoGetState(); }

  Status DoSetInterruptHandler(InterruptTrigger trigger,
                               InterruptHandler&& handler) override {
    return parent_type::DoSetInterruptHandler(trigger, std::move(handler));
  }

  Status DoEnableInterruptHandler(bool enable) override {
    return parent_type::DoEnableInterruptHandler(enable);
  }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalInOut
class ZephyrDigitalInOut
    : public DigitalInOut,
      public GenericZephyrDigitalInOut<GPIO_INPUT | GPIO_OUTPUT, false> {
 public:
  using parent_type =
      GenericZephyrDigitalInOut<GPIO_INPUT | GPIO_OUTPUT, false>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Result<State> DoGetState() override { return parent_type::DoGetState(); }

  Status DoSetState(State state) override {
    return parent_type::DoSetState(state);
  }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalOutInterrupt
class ZephyrDigitalInOutInterrupt
    : public DigitalInOutInterrupt,
      public GenericZephyrDigitalInOut<GPIO_INPUT | GPIO_OUTPUT, true> {
 public:
  using parent_type = GenericZephyrDigitalInOut<GPIO_INPUT | GPIO_OUTPUT, true>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Result<State> DoGetState() override { return parent_type::DoGetState(); }

  Status DoSetState(State state) override {
    return parent_type::DoSetState(state);
  }

  Status DoSetInterruptHandler(InterruptTrigger trigger,
                               InterruptHandler&& handler) override {
    return parent_type::DoSetInterruptHandler(trigger, std::move(handler));
  }

  Status DoEnableInterruptHandler(bool enable) override {
    return parent_type::DoEnableInterruptHandler(enable);
  }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalInterrupt
class ZephyrDigitalInterrupt
    : public DigitalInterrupt,
      public GenericZephyrDigitalInOut<GPIO_INPUT, true> {
 public:
  using parent_type = GenericZephyrDigitalInOut<GPIO_INPUT, true>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Status DoSetInterruptHandler(InterruptTrigger trigger,
                               InterruptHandler&& handler) override {
    return parent_type::DoSetInterruptHandler(trigger, std::move(handler));
  }

  Status DoEnableInterruptHandler(bool enable) override {
    return parent_type::DoEnableInterruptHandler(enable);
  }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalOut
class ZephyrDigitalOut : public DigitalOut,
                         public GenericZephyrDigitalInOut<GPIO_OUTPUT, false> {
 public:
  using parent_type = GenericZephyrDigitalInOut<GPIO_OUTPUT, false>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Status DoSetState(State state) override {
    return parent_type::DoSetState(state);
  }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

/// @brief Zephyr wrapper for pw::digital_io::DigitalOutInterrupt
class ZephyrDigitalOutInterrupt
    : public DigitalOutInterrupt,
      public GenericZephyrDigitalInOut<GPIO_INPUT | GPIO_OUTPUT, true> {
 public:
  using parent_type = GenericZephyrDigitalInOut<GPIO_INPUT | GPIO_OUTPUT, true>;
  using parent_type::GenericZephyrDigitalInOut;

 private:
  Status DoSetState(State state) override {
    return parent_type::DoSetState(state);
  }

  Status DoSetInterruptHandler(InterruptTrigger trigger,
                               InterruptHandler&& handler) override {
    return parent_type::DoSetInterruptHandler(trigger, std::move(handler));
  }

  Status DoEnableInterruptHandler(bool enable) override {
    return parent_type::DoEnableInterruptHandler(enable);
  }

  Status DoEnable(bool enable) override {
    return parent_type::DoEnable(enable);
  }
};

}  // namespace pw::digital_io
