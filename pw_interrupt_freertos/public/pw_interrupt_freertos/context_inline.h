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

#include <cstdint>

#include "portmacro.h"

namespace pw::interrupt {

// This backend require the FreeRTOS port to define the xPortIsInsideInterrupt,
// which is not available on every officially supported port. It may be
// necessary to extend certain ports for this backend to work.
inline bool InInterruptContext() { return !!xPortIsInsideInterrupt(); }

}  // namespace pw::interrupt
