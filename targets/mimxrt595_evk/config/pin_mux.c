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

#include "pin_mux.h"

#include "fsl_iopctl.h"

void BOARD_InitDEBUG_UARTPins() {
  const uint32_t G16 =
      (IOPCTL_PIO_FUNC1 | IOPCTL_PIO_PUPD_DI | IOPCTL_PIO_PULLDOWN_EN |
       IOPCTL_PIO_INBUF_DI | IOPCTL_PIO_SLEW_RATE_NORMAL |
       IOPCTL_PIO_FULLDRIVE_DI | IOPCTL_PIO_ANAMUX_DI | IOPCTL_PIO_PSEDRAIN_DI |
       IOPCTL_PIO_INV_DI);

  IOPCTL_PinMuxSet(IOPCTL, 0U, 1U, G16);

  const uint32_t H16 =
      (IOPCTL_PIO_FUNC1 | IOPCTL_PIO_PUPD_DI | IOPCTL_PIO_PULLDOWN_EN |
       IOPCTL_PIO_INBUF_EN | IOPCTL_PIO_SLEW_RATE_NORMAL |
       IOPCTL_PIO_FULLDRIVE_DI | IOPCTL_PIO_ANAMUX_DI | IOPCTL_PIO_PSEDRAIN_DI |
       IOPCTL_PIO_INV_DI);

  IOPCTL_PinMuxSet(IOPCTL, 0U, 2U, H16);

  const uint32_t N3 =
      (IOPCTL_PIO_FUNC1 | IOPCTL_PIO_PUPD_EN | IOPCTL_PIO_PULLUP_EN |
       IOPCTL_PIO_INBUF_DI | IOPCTL_PIO_SLEW_RATE_NORMAL |
       IOPCTL_PIO_FULLDRIVE_DI | IOPCTL_PIO_ANAMUX_DI | IOPCTL_PIO_PSEDRAIN_DI |
       IOPCTL_PIO_INV_DI);

  IOPCTL_PinMuxSet(IOPCTL, 2U, 24U, N3);
}

void BOARD_InitBootPins() { BOARD_InitDEBUG_UARTPins(); }
