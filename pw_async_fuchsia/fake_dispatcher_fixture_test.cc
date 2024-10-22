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

#include "pw_async/fake_dispatcher_fixture.h"

#include "pw_async_fuchsia/dispatcher.h"
#include "pw_unit_test/framework.h"

#define ASSERT_CANCELLED(status) ASSERT_EQ(Status::Cancelled(), status)

using namespace std::chrono_literals;

namespace pw::async_fuchsia {
namespace {

using FakeDispatcherFuchsiaFixture = async::test::FakeDispatcherFixture;

TEST_F(FakeDispatcherFuchsiaFixture, PostTasks) {
  int c = 0;
  auto inc_count = [&c](async::Context& /*ctx*/, Status status) {
    PW_TEST_ASSERT_OK(status);
    ++c;
  };

  async::Task task(inc_count);
  dispatcher().Post(task);

  ASSERT_EQ(c, 0);
  RunUntilIdle();
  ASSERT_EQ(c, 1);
}

TEST_F(FakeDispatcherFuchsiaFixture, DelayedTasks) {
  int c = 0;
  async::Task first([&c](async::Context& ctx, Status status) {
    PW_TEST_ASSERT_OK(status);
    c = c * 10 + 1;
  });
  async::Task second([&c](async::Context& ctx, Status status) {
    PW_TEST_ASSERT_OK(status);
    c = c * 10 + 2;
  });
  async::Task third([&c](async::Context& ctx, Status status) {
    PW_TEST_ASSERT_OK(status);
    c = c * 10 + 3;
  });

  dispatcher().PostAfter(third, 20ms);
  dispatcher().PostAfter(first, 5ms);
  dispatcher().PostAfter(second, 10ms);

  RunFor(25ms);
  EXPECT_EQ(c, 123);
}

TEST_F(FakeDispatcherFuchsiaFixture, CancelTask) {
  async::Task task([](async::Context& ctx, Status status) { FAIL(); });
  dispatcher().Post(task);
  EXPECT_TRUE(dispatcher().Cancel(task));

  RunUntilIdle();
}

TEST_F(FakeDispatcherFuchsiaFixture, HeapAllocatedTasks) {
  int c = 0;
  for (int i = 0; i < 3; i++) {
    Post(&dispatcher(), [&c](async::Context& ctx, Status status) {
      PW_TEST_ASSERT_OK(status);
      c++;
    });
  }

  RunUntilIdle();
  EXPECT_EQ(c, 3);
}

TEST_F(FakeDispatcherFuchsiaFixture, ChainedTasks) {
  int c = 0;

  Post(&dispatcher(), [&c](async::Context& ctx, Status status) {
    PW_TEST_ASSERT_OK(status);
    c++;
    Post(ctx.dispatcher, [&c](async::Context& ctx, Status status) {
      PW_TEST_ASSERT_OK(status);
      c++;
      Post(ctx.dispatcher, [&c](async::Context& ctx, Status status) {
        PW_TEST_ASSERT_OK(status);
        c++;
      });
    });
  });

  RunUntilIdle();
  EXPECT_EQ(c, 3);
}

TEST_F(FakeDispatcherFuchsiaFixture, DestroyLoopInsideTask) {
  int c = 0;
  auto inc_count = [&c](async::Context& ctx, Status status) {
    ASSERT_CANCELLED(status);
    ++c;
  };

  // These tasks are never executed and cleaned up in DestroyLoop().
  async::Task task0(inc_count), task1(inc_count);
  dispatcher().PostAfter(task0, 20ms);
  dispatcher().PostAfter(task1, 21ms);

  async::Task stop_task([&c](async::Context& ctx, Status status) {
    PW_TEST_ASSERT_OK(status);
    ++c;
    static_cast<async::test::FakeDispatcher*>(ctx.dispatcher)->RequestStop();
    // Stop has been requested; now drive the Dispatcher so it destroys the
    // loop.
    static_cast<async::test::FakeDispatcher*>(ctx.dispatcher)->RunUntilIdle();
  });
  dispatcher().Post(stop_task);

  RunUntilIdle();
  EXPECT_EQ(c, 3);
}

}  // namespace
}  // namespace pw::async_fuchsia
