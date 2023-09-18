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
#include "chre/platform/platform_debug_dump_manager.h"

namespace chre {

PlatformDebugDumpManagerBase::PlatformDebugDumpManagerBase() {}

PlatformDebugDumpManagerBase::~PlatformDebugDumpManagerBase() {}

// TODO: b/294106526 - Implement these.
void PlatformDebugDumpManager::sendDebugDump(const char*, bool) {}

void PlatformDebugDumpManager::logStateToBuffer(DebugDumpWrapper&) {}

}  // namespace chre
