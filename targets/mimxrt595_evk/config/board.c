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

#include "board.h"

#include <stdint.h>

#include "fsl_clock.h"
#include "fsl_debug_console.h"

#define BOARD_FLEXSPI_DLL_LOCK_RETRY (10)

void BOARD_InitDebugConsole(void) {
  uint32_t uartClkSrcFreq;

  /* attach FRG0 clock to FLEXCOMM0 (debug console) */
  CLOCK_SetFRGClock(BOARD_DEBUG_UART_FRG_CLK);
  CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

  uartClkSrcFreq = BOARD_DEBUG_UART_CLK_FREQ;

  DbgConsole_Init(BOARD_DEBUG_UART_INSTANCE,
                  BOARD_DEBUG_UART_BAUDRATE,
                  BOARD_DEBUG_UART_TYPE,
                  uartClkSrcFreq);
}

void BOARD_SetFlexspiClock(FLEXSPI_Type* base, uint32_t src, uint32_t divider) {
  if (base == FLEXSPI0) {
    if ((CLKCTL0->FLEXSPI0FCLKSEL != CLKCTL0_FLEXSPI0FCLKSEL_SEL(src)) ||
        ((CLKCTL0->FLEXSPI0FCLKDIV & CLKCTL0_FLEXSPI0FCLKDIV_DIV_MASK) !=
         (divider - 1))) {
      BOARD_DeinitFlash(base);

      CLKCTL0->PSCCTL0_CLR = CLKCTL0_PSCCTL0_CLR_FLEXSPI0_OTFAD_CLK_MASK;
      CLKCTL0->FLEXSPI0FCLKSEL = CLKCTL0_FLEXSPI0FCLKSEL_SEL(src);
      CLKCTL0->FLEXSPI0FCLKDIV |= CLKCTL0_FLEXSPI0FCLKDIV_RESET_MASK;
      CLKCTL0->FLEXSPI0FCLKDIV = CLKCTL0_FLEXSPI0FCLKDIV_DIV(divider - 1);
      while ((CLKCTL0->FLEXSPI0FCLKDIV) &
             CLKCTL0_FLEXSPI0FCLKDIV_REQFLAG_MASK) {
      }
      CLKCTL0->PSCCTL0_SET = CLKCTL0_PSCCTL0_SET_FLEXSPI0_OTFAD_CLK_MASK;

      BOARD_InitFlash(base);
    }
  } else if (base == FLEXSPI1) {
    if ((CLKCTL0->FLEXSPI1FCLKSEL != CLKCTL0_FLEXSPI1FCLKSEL_SEL(src)) ||
        ((CLKCTL0->FLEXSPI1FCLKDIV & CLKCTL0_FLEXSPI1FCLKDIV_DIV_MASK) !=
         (divider - 1))) {
      BOARD_DeinitFlash(base);

      CLKCTL0->PSCCTL0_CLR = CLKCTL0_PSCCTL0_CLR_FLEXSPI1_CLK_MASK;
      CLKCTL0->FLEXSPI1FCLKSEL = CLKCTL0_FLEXSPI1FCLKSEL_SEL(src);
      CLKCTL0->FLEXSPI1FCLKDIV |= CLKCTL0_FLEXSPI1FCLKDIV_RESET_MASK;
      CLKCTL0->FLEXSPI1FCLKDIV = CLKCTL0_FLEXSPI1FCLKDIV_DIV(divider - 1);
      while ((CLKCTL0->FLEXSPI1FCLKDIV) &
             CLKCTL0_FLEXSPI1FCLKDIV_REQFLAG_MASK) {
      }
      CLKCTL0->PSCCTL0_SET = CLKCTL0_PSCCTL0_SET_FLEXSPI1_CLK_MASK;

      BOARD_InitFlash(base);
    }
  } else {
    return;
  }
}

void BOARD_DeinitFlash(FLEXSPI_Type* base) {
  CLKCTL0->PSCCTL0_SET = CLKCTL0_PSCCTL0_SET_FLEXSPI0_OTFAD_CLK_MASK;

  base->MCR0 &= ~FLEXSPI_MCR0_MDIS_MASK;

  while (!((base->STS0 & FLEXSPI_STS0_ARBIDLE_MASK) &&
           (base->STS0 & FLEXSPI_STS0_SEQIDLE_MASK))) {
  }
  base->MCR0 |= FLEXSPI_MCR0_MDIS_MASK;
}

void BOARD_InitFlash(FLEXSPI_Type* base) {
  uint32_t status;
  uint32_t lastStatus;
  uint32_t retry;

  base->DLLCR[0] = 0x1U;

  base->MCR0 &= ~FLEXSPI_MCR0_MDIS_MASK;

  base->MCR0 |= FLEXSPI_MCR0_SWRESET_MASK;
  while (base->MCR0 & FLEXSPI_MCR0_SWRESET_MASK) {
  }

  if (0U != (base->DLLCR[0] & FLEXSPI_DLLCR_DLLEN_MASK)) {
    lastStatus = base->STS2;
    retry = BOARD_FLEXSPI_DLL_LOCK_RETRY;
    do {
      status = base->STS2;
      if ((status &
           (FLEXSPI_STS2_AREFLOCK_MASK | FLEXSPI_STS2_ASLVLOCK_MASK)) ==
          (FLEXSPI_STS2_AREFLOCK_MASK | FLEXSPI_STS2_ASLVLOCK_MASK)) {
        retry = 100;
        break;
      } else if (status == lastStatus) {
        retry--;
      } else {
        retry = BOARD_FLEXSPI_DLL_LOCK_RETRY;
        lastStatus = status;
      }
    } while (retry > 0);
    for (; retry > 0U; retry--) {
      __NOP();
    }
  }
}
