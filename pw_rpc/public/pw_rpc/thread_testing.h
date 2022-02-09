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
#pragma once

#include <chrono>

#include "pw_assert/assert.h"
#include "pw_rpc/internal/fake_channel_output.h"
#include "pw_sync/counting_semaphore.h"

namespace pw::rpc::test {

// Wait until the provided RawFakeChannelOutput or NanopbFakeChannelOutput
// receives the specified number of packets.
template <unsigned kTimeoutSeconds = 10, typename Function>
void WaitForPackets(internal::test::FakeChannelOutput& output,
                    int count,
                    Function&& run_before) {
  sync::CountingSemaphore sem;
  output.set_on_send([&sem](ConstByteSpan, Status) { sem.release(); });

  run_before();

  for (int i = 0; i < count; ++i) {
    PW_ASSERT(sem.try_acquire_for(std::chrono::seconds(kTimeoutSeconds)));
  }

  output.set_on_send(nullptr);
}

}  // namespace pw::rpc::test
