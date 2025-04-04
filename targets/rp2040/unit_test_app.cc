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

#include "pw_status/status.h"
#include "pw_system/rpc_server.h"
#include "pw_thread/detached_thread.h"
#include "pw_thread_freertos/context.h"
#include "pw_thread_freertos/options.h"
#include "pw_unit_test/unit_test_service.h"

namespace pw::system {

thread::freertos::StaticContextWithStack<4096> unit_test_thread_context;

const thread::Options& UnitTestThreadOptions() {
  static constexpr auto kOptions =
      thread::freertos::Options()
          .set_name("UnitTestThread")
          .set_static_context(unit_test_thread_context)
          .set_priority(tskIDLE_PRIORITY + 1);
  return kOptions;
}

unit_test::UnitTestThread unit_test_thread;

// This will run once after pw::system::Init() completes. This callback must
// return or it will block the work queue.
void UserAppInit() {
  thread::DetachedThread(UnitTestThreadOptions(), unit_test_thread);
  GetRpcServer().RegisterService(unit_test_thread.service());
}

}  // namespace pw::system
