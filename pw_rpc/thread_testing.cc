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

#include "pw_rpc/thread_testing.h"

#include "pw_sync/counting_semaphore.h"

namespace pw::rpc::test {

void WaitForPackets(internal::test::FakeChannelOutput& output, int count) {
  sync::CountingSemaphore sem;
  output.set_on_send([&sem](ConstByteSpan, Status) { sem.release(); });

  for (int i = 0; i < count; ++i) {
    sem.acquire();
  }

  output.set_on_send(nullptr);
}

}  // namespace pw::rpc::test
