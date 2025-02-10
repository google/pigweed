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

#include "clock_config.h"

#include "board.h"
#include "fsl_clock.h"
#include "fsl_power.h"

const clock_sys_pll_config_t g_sysPllConfig_BOARD_BootClockRUN = {
    .sys_pll_src = kCLOCK_SysPllXtalIn,
    .numerator = 0,
    .denominator = 1,
    .sys_pll_mult = kCLOCK_SysPllMult22};

void BOARD_InitBootClocks(void) {
  POWER_DisablePD(kPDRUNCFG_PD_LPOSC);
  CLOCK_EnableLpOscClk();

  POWER_DisablePD(kPDRUNCFG_PD_FFRO);
  CLOCK_EnableFroClk(kCLOCK_FroAllOutEn);

  BOARD_SetFlexspiClock(FLEXSPI0, 3U, 2U);

  CLOCK_SetClkDiv(kCLOCK_DivSysCpuAhbClk, 2);
  CLOCK_AttachClk(kFRO_DIV1_to_MAIN_CLK);

  POWER_DisablePD(kPDRUNCFG_PD_SYSXTAL);
  POWER_UpdateOscSettlingTime(BOARD_SYSOSC_SETTLING_US);
  CLOCK_EnableSysOscClk(true, true, BOARD_SYSOSC_SETTLING_US);
  CLOCK_SetXtalFreq(BOARD_XTAL_SYS_CLK_HZ);

  CLOCK_InitSysPll(&g_sysPllConfig_BOARD_BootClockRUN);
  CLOCK_InitSysPfd(kCLOCK_Pfd0, 24);
  CLOCK_InitSysPfd(kCLOCK_Pfd2, 24);

  CLOCK_SetClkDiv(kCLOCK_DivSysCpuAhbClk, 2U);

  CLOCK_AttachClk(kMAIN_PLL_to_MAIN_CLK);
  CLOCK_AttachClk(kMAIN_CLK_DIV_to_SYSTICK_CLK);
  CLOCK_AttachClk(kFRO_DIV2_to_CLKOUT);

  CLOCK_SetClkDiv(kCLOCK_DivPLLFRGClk, 11U);
  CLOCK_SetClkDiv(kCLOCK_DivSystickClk, 2U);
  CLOCK_SetClkDiv(kCLOCK_DivPfc0Clk, 2U);
  CLOCK_SetClkDiv(kCLOCK_DivPfc1Clk, 4U);
  CLOCK_SetClkDiv(kCLOCK_DivClockOut, 100U);

  SystemCoreClock = BOARD_BOOTCLOCKRUN_CORE_CLOCK;

  POWER_SetDeepSleepClock(kDeepSleepClk_Fro);
}
