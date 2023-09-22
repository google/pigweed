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

#include "chre/platform/power_control_manager.h"

namespace chre {

// TODO: b/301477662 - Implement these, possibly by adding a facade.
void PowerControlManager::preEventLoopProcess(size_t) {}
void PowerControlManager::postEventLoopProcess(size_t) {}
bool PowerControlManager::hostIsAwake() { return true; }

}  // namespace chre
