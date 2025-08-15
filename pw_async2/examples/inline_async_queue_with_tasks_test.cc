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

#define PW_LOG_MODULE_NAME "EXAMPLES_INLINE_ASYNC_QUEUE_WITH_TASKS"

#include <algorithm>
#include <array>

#include "pw_async2/context.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_async2/task.h"
#include "pw_async2/try.h"
#include "pw_containers/inline_async_queue.h"
#include "pw_containers/vector.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace examples {

using ::pw::InlineAsyncQueue;
using ::pw::Vector;
using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-declarations]
// Use a queue with a capacity for at most four values.
using Queue = InlineAsyncQueue<int, 4>;

// This is the fixed sequence of values the producer should output for this
// example.
constexpr std::array<int, 8> kProducerExampleData = {1, 2, 3, 4, 5, 6, 7, 8};

// Use -1 as an explicit termination indicator. An empty queue could just mean
// the producer isn't keeping up with the consumer.
constexpr int kTerminal = -1;

// This provides confirmation of what was received by the consumer
Vector<int, 10> received_by_consumer;

// A simple producer task that writes a fixed sequence of integers to a queue.
class Producer : public Task {
 public:
  // Initialize the producer state.
  explicit Producer(Queue& q) : queue_(q), remaining_(kProducerExampleData) {}

 private:
  Poll<> DoPend(Context& cx) override;

  // As a Task, the producer needs to keep track of its own state.

  // The queue is shared between producer and consumer, and is written by the
  // producer.
  Queue& queue_;

  // This span of remaining data is the mutable state for this task.
  pw::span<const int> remaining_;
};

// A simple consumer task that reads integers from a queue, and logs what was
// received.
class Consumer : public Task {
 public:
  Consumer(Queue& q) : queue_(q) {}

 private:
  Poll<> DoPend(Context& cx) override;

  Queue& queue_;
};
// DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-declarations]

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-producer-do-pend]
Poll<> Producer::DoPend(Context& cx) {
  // This task's DoPend() is called repeatedly as long as it returns
  // `Pending().`

  // Log that this DoPend() was invoked.
  PW_LOG_INFO("Producer::DoPend() invoked");

  // Loop while we have values to write, to write them all.
  while (!remaining_.empty()) {
    // PW_TRY_READY delegates to an existing async2 aware function that
    // returns a `Poll<>` result.
    //
    // If the function delegated to returns `Pending()`, the macro will cause
    // the current function to return `Pending()`, and this task will go on
    // the sleeping list. Other tasks that aren't in the sleeping state will
    // get a chance to run.
    //
    // Here `InlineAsyncQueue::PendHasSpace` checks if there is space
    // available in the queue. If there is no space, it internally will save a
    // copy of the waker for the task before returning `Pending()`. It will
    // later invoke the waker once there is room in the queue.
    PW_TRY_READY(queue_.PendHasSpace(cx));

    // If the previous PW_TRY_READY(...) didn't force a return, it means there
    // is space in the queue. Write the next item from the remaining data to
    // the queue.
    int value = *remaining_.begin();
    PW_LOG_INFO("Producer: writing: %d", value);
    queue_.push(value);

    // Update the remaining state
    remaining_ = remaining_.subspan(1);

    // Continue the while loop to try writing more values to the queue
    // immediately.
  }

  // If we are out of data, write the termination sentinel value.
  PW_TRY_READY(queue_.PendHasSpace(cx));
  PW_LOG_INFO("Producer: writing terminal");
  queue_.push(kTerminal);

  PW_LOG_INFO("Producer: completed");
  // Return Ready() to signal this task is done.
  return Ready();
}
// DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-producer-do-pend]

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-consumer-do-pend]
Poll<> Consumer::DoPend(Context& cx) {
  // This task's DoPend() is called repeatedly as long as it returns
  // `Pending()`.

  // Log that this DoPend() was invoked.
  PW_LOG_INFO("Consumer::DoPend() invoked");

  // We loop forever, trying to read values, however really the loop is broken
  // by two conditions....
  while (true) {
    // As with the producer, this PW_TRY_READY delegates to an existing async2
    // aware function that is part of the `InlineAsyncQueue` interface.
    //
    // Here `InlineAsyncQueue::PendNotEmpty` checks if there are values in the
    // queue, and returns a `Poll<>` result. If there is no data, it internally
    // will save a copy of the waker for the task before returning the
    // `Pending()` state. It will later invoke the waker once something is added
    // to the queue.
    PW_TRY_READY(queue_.PendNotEmpty(cx));

    // If the previous PW_TRY_READY(...) didn't force a return, it means there
    // are values to read from the queue. Read the next item from the queue.

    // Obtain the value at the front of the queue and record it.
    int result = queue_.front();
    queue_.pop();
    PW_LOG_INFO("Consumer: read %d", result);
    received_by_consumer.push_back(result);

    // If we read the termination value, we can stop.
    if (result == kTerminal) {
      PW_LOG_INFO("Consumer: completed");

      // Return Ready() to signal this task is done.
      return Ready();
    }

    // Continue the while loop to try reading more values from the queue
    // immediately.
  }

  // The while loop above will return if appropriate.
  PW_UNREACHABLE;
}
// DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-consumer-do-pend]

int main() {
  // DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-run]
  // The dispatcher handles dispatching to all tasks.
  Dispatcher dispatcher;

  // The queue shared by both the producer and consumer.
  Queue queue;

  // Instantiate the producer and consumer, pointing them at the queue.
  Producer producer(queue);
  Consumer consumer(queue);

  // Register the producer and consumer with the dispatcher.
  dispatcher.Post(producer);
  dispatcher.Post(consumer);

  // Run until all tasks return `Ready()`.
  dispatcher.RunToCompletion();
  // DOCSTAG: [pw_async2-examples-inline-async-queue-with-tasks-run]

  return 0;
}

}  // namespace examples

namespace {

TEST(ExampleTests, InlineAsyncQueueWithTasks) {
  using namespace examples;

  main();

  EXPECT_EQ(examples::received_by_consumer.size(), 9U);
  EXPECT_TRUE(std::equal(kProducerExampleData.begin(),
                         kProducerExampleData.end(),
                         received_by_consumer.begin()));
  EXPECT_EQ(received_by_consumer[8U], kTerminal);
}

}  // namespace
