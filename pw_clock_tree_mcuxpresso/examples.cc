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

#include "pw_clock_tree_mcuxpresso/clock_tree.h"

// Test headers
#include "pw_unit_test/framework.h"

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Flexcomm0]

// Define FRO_DIV_4 clock source
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoFro fro_div4(kCLOCK_FroDiv4OutEn);

// Define FRG0 configuration
constexpr clock_frg_clk_config_t kFrg0Config = {
    .num = 0,
    .sfg_clock_src = _clock_frg_clk_config::kCLOCK_FrgFroDiv4,
    .divider = 255U,
    .mult = 144,
};

PW_CONSTINIT pw::clock_tree::ClockMcuxpressoFrgNonBlocking frg_0(fro_div4,
                                                                 kFrg0Config);

// Define clock source selector FLEXCOMM0
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoSelectorNonBlocking
    flexcomm_selector_0(frg_0, kFRG_to_FLEXCOMM0, kNONE_to_FLEXCOMM0);

// Define clock source clock ip name kCLOCK_Flexcomm0
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoClockIpNonBlocking flexcomm_0(
    flexcomm_selector_0, kCLOCK_Flexcomm0);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Flexcomm0]

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-fro_div8]

// Define FRO_DIV8 clock source
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoFro fro_div8(kCLOCK_FroDiv8OutEn);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-fro_div8]

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-i3c0]

// Define clock source selector I3C01FCLKSEL
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoSelectorNonBlocking i3c0_selector(
    fro_div8, kFRO_DIV8_to_I3C_CLK, kNONE_to_I3C_CLK);

// Define clock divider I3C01FCLKDIV
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoDividerNonBlocking i3c0_divider(
    i3c0_selector, kCLOCK_DivI3cClk, 12);

// Define clock source clock ip name kCLOCK_I3c0
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoClockIpNonBlocking i3c0(
    i3c0_divider, kCLOCK_I3c0);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-i3c0]

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClkTreeElemDefs-ClockSourceNoOp]

// Need to define `ClockSourceNoOp` clock tree element to satisfy dependency for
// `ClockMcuxpressoMclk` or `ClockMcuxpressoClkIn` class.
PW_CONSTINIT pw::clock_tree::ClockSourceNoOp clock_source_no_op;

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClkTreeElemDefs-ClockSourceNoOp]

// inclusive-language: disable
// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Ctimer0]

// Define Master clock
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoMclkNonBlocking mclk(
    clock_source_no_op, 19200000);

// Define clock selector CTIMER0
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoSelectorNonBlocking
    ctimer_selector_0(mclk, kMASTER_CLK_to_CTIMER0, kNONE_to_CTIMER0);

// Define clock source clock ip name kCLOCK_Ct32b0
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoClockIpNonBlocking ctimer_0(
    ctimer_selector_0, kCLOCK_Ct32b0);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Ctimer0]
// inclusive-language: enable

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-LpOsc]

// Define Low-Power Oscillator
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoLpOsc lp_osc_clk;

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-LpOsc]

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPll]

// Define ClkIn pin clock source
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoClkInNonBlocking clk_in(
    clock_source_no_op, 19200000);

// Define audio PLL configuration with ClkIn pin as clock source
const clock_audio_pll_config_t kAudioPllConfig = {
    .audio_pll_src = kCLOCK_AudioPllXtalIn, /* OSC clock */
    .numerator =
        0, /* Numerator of the Audio PLL fractional loop divider is 0 */
    .denominator =
        1000, /* Denominator of the Audio PLL fractional loop divider is 1 */
    .audio_pll_mult = kCLOCK_AudioPllMult16 /* Divide by 16 */
};

// Define Audio PLL sourced by ClkIn pin clock source
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoAudioPllNonBlocking audio_pll(
    clk_in, kAudioPllConfig, 18);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPll]

// DOCSTAG:[pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPllBypass]

// Define Audio PLL in bypass mode sourced by FRO_DIV8 clock source
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoAudioPllNonBlocking
    audio_pll_bypass(fro_div8, kCLOCK_AudioPllFroDiv8Clk);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPllBypass]

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-SysPll]

// SysPLL configuration with ClkIn pin as clock source
const clock_sys_pll_config_t kSysPllConfig = {
    .sys_pll_src = kCLOCK_SysPllXtalIn, /* OSC clock */
    .numerator = 0, /* Numerator of the SYSPLL0 fractional loop divider is 0 */
    .denominator =
        1, /* Denominator of the SYSPLL0 fractional loop divider is 1 */
    .sys_pll_mult = kCLOCK_SysPllMult20 /* Divide by 20 */
};

// Define Sys PLL sourced by ClkIn pin clock source
PW_CONSTINIT pw::clock_tree::ClockMcuxpressoSysPllNonBlocking sys_pll(
    clk_in, kSysPllConfig, 18, 0, 0, 0);

// DOCSTAG: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-SysPll]

PW_CONSTINIT pw::clock_tree::ClockMcuxpressoRtcNonBlocking rtc(
    clock_source_no_op);

TEST(ClockTreeMcuxpresso, UseExample) {
  // DOCSTAG: [pw_clock_tree_mcuxpresso-examples-UseExample]

  // Enable the low-power oscillator
  lp_osc_clk.Acquire();

  // Enable the i3c0
  i3c0.Acquire();

  // Change the i3c0_divider value
  i3c0_divider.SetDivider(24);

  // Enable the flexcomm0 interface
  flexcomm_0.Acquire();

  // Disable the low-power oscillator
  lp_osc_clk.Release();

  // DOCSTAG: [pw_clock_tree_mcuxpresso-examples-UseExample]
}

TEST(ClockTreeMcuxpresso, AudioPll) {
  // DOCSTAG:[pw_clock_tree_mcuxpresso-examples-Use-AudioPll]

  // Enable audio PLL. We use AcquireWith to ensure that FRO_DIV8
  // is enabled while enabling the audio PLL. If FRO_DIV8 wasn't enabled
  // before, it will only be enabled while configuring the audio PLL
  // and be disabled afterward to save power.
  PW_TEST_EXPECT_OK(audio_pll.AcquireWith(fro_div8));

  // Do something while audio PLL is enabled.

  // Release audio PLL to save power.
  audio_pll.Release();
  // DOCSTAG:[pw_clock_tree_mcuxpresso-examples-Use-AudioPll]
}

TEST(ClockTreeMcuxpresso, SysPll) {
  // DOCSTAG:[pw_clock_tree_mcuxpresso-examples-Use-SysPll]

  // Enable sys PLL.
  sys_pll.Acquire();

  // Do something while sys PLL is enabled.

  // Release audio PLL to save power.
  sys_pll.Release();
  // DOCSTAG:[pw_clock_tree_mcuxpresso-examples-Use-sysPll]
}

TEST(ClockTreeMcuxpresso, AudioPllBypass) {
  audio_pll_bypass.Acquire();
  audio_pll_bypass.Release();
}

TEST(ClockTreeMcuxpresso, Rtc) {
  rtc.Acquire();
  rtc.Release();
}
