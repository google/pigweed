// Copyright 2022 The Pigweed Authors
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
#define PW_LOG_MODULE_NAME "pw_perf_test"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_perf_test/perf_test.h"

#include "pw_perf_test/event_handler.h"
#include "pw_perf_test/internal/framework.h"
#include "pw_perf_test/internal/timer.h"

namespace pw::perf_test {

void RunAllTests(EventHandler& handler) {
  internal::Framework::Get().RegisterEventHandler(handler);
  internal::Framework::Get().RunAllTests();
}

}  // namespace pw::perf_test
