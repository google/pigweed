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
#include "pw_chre/chre.h"
#include "pw_system/target_hooks.h"
#include "pw_thread/thread.h"

namespace pw::system {
namespace {

pw::thread::Thread chre_thread;

}  // namespace

// This will run once after pw::system::Init() completes. This callback must
// return or it will block the work queue.
void UserAppInit() {
  // Start the thread that is running CHRE.
  chre_thread = pw::thread::Thread(
      pw::system::LogThreadOptions(),
      [](void*) {
        pw::chre::Init();
        pw::chre::RunEventLoop();
        pw::chre::Deinit();
      },
      nullptr);
}

}  // namespace pw::system
