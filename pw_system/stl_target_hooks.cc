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

#include "pw_system/config.h"
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

#if PW_SYSTEM_ENABLE_TRANSFER_SERVICE
const thread::Options& TransferThreadOptions() {
  static thread::stl::Options transfer_thread_options;
  return transfer_thread_options;
}
#endif  // PW_SYSTEM_ENABLE_TRANSFER_SERVICE

const thread::Options& WorkQueueThreadOptions() {
  static thread::stl::Options work_queue_thread_options;
  return work_queue_thread_options;
}

}  // namespace pw::system
