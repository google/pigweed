// Copyright 2021 The Pigweed Authors
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

#include "FreeRTOS.h"
#include "pw_log/log.h"
#include "pw_system/init.h"
#include "pw_thread/detached_thread.h"
#include "pw_thread/thread.h"
#include "pw_thread/thread_core.h"
#include "pw_thread_freertos/context.h"

namespace {

class IdleThread : public pw::thread::ThreadCore {
 public:
  IdleThread() = default;
  ~IdleThread() = default;
  IdleThread(const IdleThread&) = delete;
  IdleThread(IdleThread&&) = delete;
  IdleThread& operator=(const IdleThread&) = delete;
  IdleThread& operator=(IdleThread&&) = delete;

  // From pw::thread::ThreadCore
  void Run() final {
    while (true) {
      PW_LOG_INFO("The cake is a lie!");
      vTaskDelay(1000);
    }
  }
};

IdleThread idle_thread;
std::array<StackType_t, 512> idle_stack;
pw::thread::freertos::StaticContext idle_context(idle_stack);
constexpr pw::thread::freertos::Options idle_thread_options =
    pw::thread::freertos::Options()
        .set_name("IdleThread")
        .set_static_context(idle_context)
        .set_priority(tskIDLE_PRIORITY + 2);

}  // namespace

extern "C" int main() {
  PW_LOG_INFO("Demo app");
  pw::system::Init();
  // Hand off the main stack to the kernel.
  pw::thread::DetachedThread(idle_thread_options, idle_thread);
  vTaskStartScheduler();
}
