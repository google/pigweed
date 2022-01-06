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

#define PW_LOG_MODULE_NAME "SYS"

#include "pw_log/log.h"
#include "pw_system/init.h"
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

namespace pw::system {

const thread::Options& LogThreadOptions() {
  static thread::stl::Options log_thread_options;
  return log_thread_options;
}

const thread::Options& RpcThreadOptions() {
  static thread::stl::Options rpc_thread_options;
  return rpc_thread_options;
}

const thread::Options& WorkQueueThreadOptions() {
  static thread::stl::Options work_queue_thread_options;
  return work_queue_thread_options;
}

}  // namespace pw::system

extern "C" int main() {
  pw::system::Init();
  // Sleep loop rather than return on this thread so the process isn't closed.
  while (true) {
    pw::this_thread::sleep_for(std::chrono::seconds(10));
    // It's hard to tell that simulator is alive and working since nothing is
    // logging after initial "boot," so for now log a line occasionally so
    // users can see that the simulator is alive and well.
    PW_LOG_INFO("Simulated device is still alive");
    // TODO(amontanez): This thread should probably have a way to exit.
  }
}
