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

#include "fsl_clock.h"
#include "fsl_power.h"
#include "pw_clock_tree/clock_tree.h"

namespace pw::clock_tree {

/// Class implementing an FRO clock source.
class ClockMcuxpressoFro final
    : public ClockSource<ElementNonBlockingCannotFail> {
 public:
  /// Constructor specifying the FRO divider output to manage.
  constexpr ClockMcuxpressoFro(clock_fro_output_en_t fro_output)
      : fro_output_(fro_output) {}

 private:
  /// Enable this FRO divider.
  pw::Status DoEnable() final {
    CLOCK_EnableFroClk(CLKCTL0->FRODIVOEN | fro_output_);
    return pw::OkStatus();
  }

  /// Disable this FRO divider.
  pw::Status DoDisable() final {
    CLOCK_EnableFroClk(CLKCTL0->FRODIVOEN & ~fro_output_);
    return pw::OkStatus();
  }

  /// FRO divider.
  const uint32_t fro_output_;
};

/// Class implementing the low power oscillator clock source.
class ClockMcuxpressoLpOsc final
    : public ClockSource<ElementNonBlockingCannotFail> {
 private:
  /// Enable low power oscillator.
  pw::Status DoEnable() final {
    POWER_DisablePD(kPDRUNCFG_PD_LPOSC); /* Power on LPOSC (1MHz) */
    // POWER_ApplyPD() is not necessary for LPOSC_PD.
    CLOCK_EnableLpOscClk(); /* Wait until LPOSC stable */
    return pw::OkStatus();
  }

  /// Disable low power oscillator.
  pw::Status DoDisable() final {
    POWER_EnablePD(kPDRUNCFG_PD_LPOSC); /* Power down LPOSC (1MHz). */
    // POWER_ApplyPD() is not necessary for LPOSC_PD.
    return pw::OkStatus();
  }
};

/// Class template implementing the MCLK IN clock source.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoMclk final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the MCLK IN clock frequency in Hz and
  /// the dependent clock tree element to enable the MCLK clock source.
  constexpr ClockMcuxpressoMclk(ElementType& source, uint32_t frequency)
      : DependentElement<ElementType>(source), frequency_(frequency) {}

 private:
  /// Set MCLK IN clock frequency.
  pw::Status DoEnable() final {
    CLOCK_SetMclkFreq(frequency_); /* Sets external MCLKIN freq */
    return pw::OkStatus();
  }

  /// Set MCLK IN clock frequency to 0 Hz.
  pw::Status DoDisable() final {
    CLOCK_SetMclkFreq(0); /* Sets external MCLKIN freq */
    return pw::OkStatus();
  }

  /// MCLK IN frequency.
  uint32_t frequency_;
};

/// Alias for a blocking MCLK IN clock tree element.
using ClockMcuxpressoMclkBlocking = ClockMcuxpressoMclk<ElementBlocking>;

/// Alias for a non-blocking MCLK IN clock tree element where updates cannot
/// fail.
using ClockMcuxpressoMclkNonBlocking =
    ClockMcuxpressoMclk<ElementNonBlockingCannotFail>;

/// Class template implementing the CLK IN pin clock source.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoClkIn final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the CLK IN pin clock frequency in Hz and
  /// the dependent clock tree element to enable the CLK IN pin clock source.
  constexpr ClockMcuxpressoClkIn(ElementType& source, uint32_t frequency)
      : DependentElement<ElementType>(source), frequency_(frequency) {}

 private:
  /// Set CLK IN clock frequency.
  pw::Status DoEnable() final {
    CLOCK_SetClkinFreq(
        frequency_); /*!< Sets CLK_IN pin clock frequency in Hz */
    return pw::OkStatus();
  }

  /// Set CLK IN clock frequency to 0 Hz.
  pw::Status DoDisable() final {
    CLOCK_SetClkinFreq(0); /*!< Sets CLK_IN pin clock frequency in Hz */
    return pw::OkStatus();
  }

  /// CLK IN frequency.
  uint32_t frequency_;
};

/// Alias for a blocking CLK IN pin clock tree element.
using ClockMcuxpressoClkInBlocking = ClockMcuxpressoClkIn<ElementBlocking>;

/// Alias for a non-blocking CLK IN pin clock tree element where updates cannot
/// fail.
using ClockMcuxpressoClkInNonBlocking =
    ClockMcuxpressoClkIn<ElementNonBlockingCannotFail>;

/// Class template implementing the FRG clock tree element.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoFrg final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the source clock and FRG configuration.
  constexpr ClockMcuxpressoFrg(ElementType& source,
                               const clock_frg_clk_config_t& config)
      : DependentElement<ElementType>(source), config_(config) {}

 private:
  // FRG clock source selector None, which gates output to reduce power.
  // The None source selector is not defined in the SDK.
  const uint8_t kCLOCK_FrgNone = 7;

  /// Enable FRG configuration.
  pw::Status DoEnable() final {
    CLOCK_SetFRGClock(&config_);
    return pw::OkStatus();
  }

  /// Disable FRG configuration.
  pw::Status DoDisable() final {
    clock_frg_clk_config_t disable_config = config_;
    static_assert(sizeof(disable_config.sfg_clock_src) ==
                  sizeof(kCLOCK_FrgNone));
    disable_config.sfg_clock_src =
        static_cast<decltype(disable_config.sfg_clock_src)>(kCLOCK_FrgNone);
    CLOCK_SetFRGClock(&disable_config);
    return pw::OkStatus();
  }

  /// FRG clock configuration to enable FRG component.
  const clock_frg_clk_config_t& config_;
};

/// Alias for a blocking FRG clock tree element.
using ClockMcuxpressoFrgBlocking = ClockMcuxpressoFrg<ElementBlocking>;

/// Alias for a non-blocking FRG clock tree element where updates cannot fail.
using ClockMcuxpressoFrgNonBlocking =
    ClockMcuxpressoFrg<ElementNonBlockingCannotFail>;

/// Class template implementing the clock selector element.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoSelector : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the source clock and the selector value
  /// when the selector should get enabled, and the selector value when
  /// the selector should get disabled to save power.
  constexpr ClockMcuxpressoSelector(ElementType& source,
                                    clock_attach_id_t selector_enable,
                                    clock_attach_id_t selector_disable)
      : DependentElement<ElementType>(source),
        selector_enable_(selector_enable),
        selector_disable_(selector_disable) {}

 private:
  /// Enable selector.
  pw::Status DoEnable() final {
    CLOCK_AttachClk(selector_enable_);
    return pw::OkStatus();
  }

  /// Disable selector.
  pw::Status DoDisable() final {
    CLOCK_AttachClk(selector_disable_);
    return pw::OkStatus();
  }

  /// Enable selector value.
  clock_attach_id_t selector_enable_;
  /// Disable selector value.
  clock_attach_id_t selector_disable_;
};

/// Alias for a blocking clock selector clock tree element.
using ClockMcuxpressoSelectorBlocking =
    ClockMcuxpressoSelector<ElementBlocking>;

/// Alias for a non-blocking clock selector clock tree element where updates
/// cannot fail.
using ClockMcuxpressoSelectorNonBlocking =
    ClockMcuxpressoSelector<ElementNonBlockingCannotFail>;

/// Class template implementing the clock divider element.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoDivider final : public ClockDividerElement<ElementType> {
 public:
  /// Constructor specifying the source clock, the name of the divder and
  /// the divider setting.
  constexpr ClockMcuxpressoDivider(ElementType& source,
                                   clock_div_name_t divider_name,
                                   uint32_t divider)
      : ClockDividerElement<ElementType>(source, divider),
        divider_name_(divider_name) {}

 private:
  /// Set the divider configuration.
  pw::Status DoEnable() final {
    CLOCK_SetClkDiv(divider_name_, this->divider());
    return pw::OkStatus();
  }

  /// Name of divider.
  clock_div_name_t divider_name_;
};

/// Alias for a blocking clock divider clock tree element.
using ClockMcuxpressoDividerBlocking = ClockMcuxpressoDivider<ElementBlocking>;

/// Alias for a non-blocking clock divider clock tree element where updates
/// cannot fail.
using ClockMcuxpressoDividerNonBlocking =
    ClockMcuxpressoDivider<ElementNonBlockingCannotFail>;

}  // namespace pw::clock_tree
