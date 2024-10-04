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

#include "pw_async2/coro.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/simulated_time_provider.h"
#include "pw_chrono/system_clock.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::TimeProvider;
using ::pw::chrono::SystemClock;
using ::std::chrono_literals::operator""ms;

class Led {
 public:
  void TurnOn() {}
  void TurnOff() {}
};

// DOCSTAG: [pw_async2-examples-coro-blinky-loop]
Coro<Status> Blink(CoroContext&,
                   TimeProvider<SystemClock>& time,
                   Led& led,
                   int times) {
  for (int i = 0; i < times; ++i) {
    led.TurnOn();
    co_await time.WaitFor(500ms);
    led.TurnOff();
    co_await time.WaitFor(500ms);
  }
  led.TurnOff();
  co_return OkStatus();
}
// DOCSTAG: [pw_async2-examples-coro-blinky-loop]

}  // namespace

#include "pw_allocator/testing.h"

namespace {

using ::pw::OkStatus;
using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Context;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::Dispatcher;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::SimulatedTimeProvider;
using ::pw::async2::Task;

class ExpectCoroTask final : public Task {
 public:
  ExpectCoroTask(Coro<pw::Status>&& coro) : coro_(std::move(coro)) {}

 private:
  Poll<> DoPend(Context& cx) final {
    Poll<Status> result = coro_.Pend(cx);
    if (result.IsPending()) {
      return Pending();
    }
    EXPECT_EQ(*result, OkStatus());
    return Ready();
  }
  Coro<pw::Status> coro_;
};

TEST(CoroExample, ReturnsOk) {
  AllocatorForTest<512> alloc;
  CoroContext coro_cx(alloc);
  SimulatedTimeProvider<SystemClock> time;
  Led led;
  ExpectCoroTask task = Blink(coro_cx, time, led, /*times=*/3);
  Dispatcher dispatcher;
  dispatcher.Post(task);
  while (dispatcher.RunUntilStalled().IsPending()) {
    time.AdvanceUntilNextExpiration();
  }
}

}  // namespace
