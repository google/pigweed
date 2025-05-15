// Copyright 2025 The Pigweed Authors
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

#include "pw_async2/waker_queue.h"

#include "pw_async2/context.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/waker.h"
#include "pw_unit_test/framework.h"

namespace pw::async2 {
namespace {

class QueuedReader {
 public:
  Poll<int> ReadValue(Context& cx) {
    if (result.has_value()) {
      return Ready(result.value());
    }

    PW_ASYNC_STORE_WAKER(cx, queue, "Task is blocked on queue");
    return Pending();
  }

  void SetValueAndWakeMany(int value, size_t tasks_to_wake) {
    result = value;
    queue.WakeMany(tasks_to_wake);
  }

  void SetValueAndWakeOne(int value) {
    result = value;
    queue.WakeOne();
  }

  void SetValueAndWakeAll(int value) {
    result = value;
    queue.WakeAll();
  }

  WakerQueue<4> queue;
  std::optional<int> result;
};

class ReaderTask : public Task {
 public:
  int value = 0;

  ReaderTask(QueuedReader& reader)
      : Task(PW_ASYNC_TASK_NAME("ReaderTask")), reader_(reader) {}

 private:
  Poll<> DoPend(Context& cx) override {
    Poll<int> result = reader_.ReadValue(cx);
    if (result.IsPending()) {
      return Pending();
    }

    value = *result;
    return Ready();
  }

  QueuedReader& reader_;
};

TEST(WakerQueue, Empty) {
  WakerQueue<4> queue;
  EXPECT_TRUE(queue.empty());

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& cx) {
    PW_ASYNC_STORE_WAKER(cx, queue, "Storing waker in queue");
    return Pending();
  });
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(task), Pending());

  EXPECT_EQ(queue.size(), 1u);
  EXPECT_FALSE(queue.empty());

  queue.WakeAll();
}

TEST(WakerQueue, Full) {
  WakerQueue<1> queue;
  EXPECT_FALSE(queue.full());

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& cx) {
    PW_ASYNC_STORE_WAKER(cx, queue, "Storing waker in queue");
    return Pending();
  });
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(task), Pending());

  EXPECT_EQ(queue.size(), 1u);
  EXPECT_TRUE(queue.full());

  queue.WakeAll();
}

TEST(WakerQueue, AddEmptyWakerFails) {
  WakerQueue<1> queue;
  EXPECT_FALSE(queue.Add(Waker()));
  EXPECT_TRUE(queue.empty());
}

TEST(WakerQueue, TryStore) {
  WakerQueue<1> queue;
  Dispatcher dispatcher;

  PendFuncTask task_1([&](Context& cx) {
    EXPECT_TRUE(PW_ASYNC_TRY_STORE_WAKER(cx, queue, "Task 1 storing waker"));
    return Pending();
  });
  PendFuncTask task_2([&](Context& cx) {
    EXPECT_FALSE(PW_ASYNC_TRY_STORE_WAKER(cx, queue, "Task 2 storing waker"));
    return Ready();
  });

  dispatcher.Post(task_1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  dispatcher.Post(task_2);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  queue.WakeAll();
}

TEST(WakerQueue, WakeOne) {
  Dispatcher dispatcher;
  QueuedReader reader;
  ReaderTask reader_task_1(reader);
  ReaderTask reader_task_2(reader);

  dispatcher.Post(reader_task_1);
  dispatcher.Post(reader_task_2);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(reader.queue.size(), 2u);

  reader.SetValueAndWakeOne(7);
  EXPECT_EQ(reader.queue.size(), 1u);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(reader_task_1.value, 7);
  EXPECT_EQ(reader_task_2.value, 0);

  reader.SetValueAndWakeOne(9);
  EXPECT_TRUE(reader.queue.empty());
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(reader_task_1.value, 7);
  EXPECT_EQ(reader_task_2.value, 9);
}

TEST(WakerQueue, WakeMany) {
  Dispatcher dispatcher;
  QueuedReader reader;
  ReaderTask reader_task_1(reader);
  ReaderTask reader_task_2(reader);
  ReaderTask reader_task_3(reader);

  dispatcher.Post(reader_task_1);
  dispatcher.Post(reader_task_2);
  dispatcher.Post(reader_task_3);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(reader.queue.size(), 3u);

  reader.SetValueAndWakeMany(7, /*tasks_to_wake=*/2);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(reader.queue.size(), 1u);
  EXPECT_EQ(reader_task_1.value, 7);
  EXPECT_EQ(reader_task_2.value, 7);
  EXPECT_EQ(reader_task_3.value, 0);

  reader.SetValueAndWakeMany(9, /*tasks_to_wake=*/2);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(reader.queue.empty());
  EXPECT_EQ(reader_task_1.value, 7);
  EXPECT_EQ(reader_task_2.value, 7);
  EXPECT_EQ(reader_task_3.value, 9);
}

TEST(WakerQueue, WakeAll) {
  Dispatcher dispatcher;
  QueuedReader reader;
  ReaderTask reader_task_1(reader);
  ReaderTask reader_task_2(reader);
  ReaderTask reader_task_3(reader);

  dispatcher.Post(reader_task_1);
  dispatcher.Post(reader_task_2);
  dispatcher.Post(reader_task_3);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(reader.queue.size(), 3u);

  reader.SetValueAndWakeAll(6);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(reader.queue.empty());
  EXPECT_EQ(reader_task_1.value, 6);
  EXPECT_EQ(reader_task_2.value, 6);
  EXPECT_EQ(reader_task_3.value, 6);

  reader.SetValueAndWakeAll(12);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(reader.queue.empty());
  EXPECT_EQ(reader_task_1.value, 6);
  EXPECT_EQ(reader_task_2.value, 6);
  EXPECT_EQ(reader_task_3.value, 6);
}

}  // namespace
}  // namespace pw::async2
