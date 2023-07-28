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

#include "pw_interrupt/context.h"

#include <xtensa/config/core.h>
#include <xtensa/hal.h>

#include <cstdint>

namespace pw::interrupt {

bool InInterruptContext() {
  // xthal_intlevel_get returns the current interrupt level of the processor
  // (value of PS.INTLEVEL register). C based handlers are always dispatched
  // from an interrupt level below XCHAL_EXCM_LEVEL - handlers running above
  // this level must be written in assembly. The interrupt level is set to zero
  // when interrupts are enabled but the core isn't currently procesing one.
  const uint32_t int_level = xthal_intlevel_get();
  return (int_level < XCHAL_EXCM_LEVEL) && (int_level > 0);
}

}  // namespace pw::interrupt
