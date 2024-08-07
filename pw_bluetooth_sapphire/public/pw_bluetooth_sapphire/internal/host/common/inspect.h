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

#pragma once

#include "pw_bluetooth_sapphire/config.h"

#ifdef NINSPECT
#include "pw_bluetooth_sapphire/internal/host/common/fake_inspect.h"
#else
#include "pw_preprocessor/compiler.h"
PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
#include <lib/inspect/cpp/inspect.h>
PW_MODIFY_DIAGNOSTICS_POP();
#endif  // NINSPECT
