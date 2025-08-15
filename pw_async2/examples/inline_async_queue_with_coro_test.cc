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

#define PW_LOG_MODULE_NAME "EXAMPLES_INLINE_ASYNC_QUEUE_WITH_CORO"

#include <algorithm>
#include <array>

#include "pw_allocator/libc_allocator.h"
#include "pw_async2/context.h"
#include "pw_async2/coro.h"
#include "pw_async2/coro_or_else_task.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_awaitable.h"
#include "pw_containers/inline_async_queue.h"
#include "pw_containers/vector.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace examples {

using ::pw::InlineAsyncQueue;
using ::pw::OkStatus;
using ::pw::Status;
using ::pw::Vector;
using ::pw::allocator::LibCAllocator;
using ::pw::async2::Context;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::CoroOrElseTask;
using ::pw::async2::Dispatcher;
using ::pw::async2::PendFuncAwaitable;

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-declarations]
// Use a queue with a capacity for at most four integers.
using Queue = InlineAsyncQueue<int, 4>;

// This is the fixed sequence of values the producer should output for this
// example.
constexpr std::array<int, 8> kProducerExampleData = {1, 2, 3, 4, 5, 6, 7, 8};

// Use -1 as a termination indicator, as an empty queue could just mean the
// producer isn't keeping up with the consumer.
constexpr int kTerminal = -1;

// This provides confirmation of what was received by the consumer
Vector<int, 10> received_by_consumer;

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-declarations]

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-adapters]
// Constructs a co-awaitable value to wait for the queue to have free space.
auto QueueHasSpace(Queue& queue) {
  return PendFuncAwaitable(
      [&queue](Context& cx) { return queue.PendHasSpace(cx); });
}

// Constructs a co-awaitable value to wait for the queue to have content.
auto QueueNotEmpty(Queue& queue) {
  return PendFuncAwaitable(
      [&queue](Context& cx) { return queue.PendNotEmpty(cx); });
}
// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-adapters]

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-producer]
// A simple producer coroutine that writes a fixed sequence of integers to a
// queue.
Coro<Status> Producer(CoroContext&, Queue& queue) {
  PW_LOG_INFO("Producer() invoked");

  // Loop over all the values to output.
  for (auto value : kProducerExampleData) {
    // Wait for there to be space in the queue before writing the next value.
    co_await QueueHasSpace(queue);
    PW_LOG_INFO("Producer() output %d", value);
    queue.push(value);
  }

  // Once we are out of values, write the termination sentinel value.

  // Wait for there to be space in the queue before writing kTerminal.
  co_await QueueHasSpace(queue);
  PW_LOG_INFO("Producer() output terminal");
  queue.push(kTerminal);

  PW_LOG_INFO("Producer() complete");
  co_return OkStatus();
}
// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-producer]

// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-consumer]
// A simple reader coroutine that reads integers from a queue, and logs them
// out.
Coro<Status> Consumer(CoroContext&, Queue& queue) {
  PW_LOG_INFO("Consumer() invoked");

  while (true) {
    // Wait for there to be something to read.
    co_await QueueNotEmpty(queue);

    // Obtain a value from the queue, and record it.
    int result = queue.front();
    queue.pop();
    PW_LOG_INFO("Consumer() input: %d", result);
    received_by_consumer.push_back(result);

    // If we read the termination value, we can stop.
    if (result == kTerminal) {
      PW_LOG_INFO("Consumer() complete");
      // Signal we are done.
      co_return OkStatus();
    }
  }

  // The while loop above will return if appropriate.
  PW_UNREACHABLE;
}
// DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-consumer]

int main() {
  // DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-run]
  // The dispatcher handles dispatching to all tasks.
  Dispatcher dispatcher;

  // The CoroContext needs an allocator instance. Use the libc allocator.
  LibCAllocator alloc;

  // Creating a CoroContext is required before using coroutines. It makes
  // the memory allocations needed for each coroutine at runtime when each is
  // started.
  CoroContext coro_cx(alloc);

  // The queue shared by both the producer and consumer.
  Queue queue;

  // Instantiate the producer and consumer, pointing them at the queue.
  auto producer = CoroOrElseTask(Producer(coro_cx, queue), [](Status) {});
  auto consumer = CoroOrElseTask(Consumer(coro_cx, queue), [](Status) {});

  // Register the producer and consumer with the dispatcher.
  dispatcher.Post(producer);
  dispatcher.Post(consumer);

  // Run until all tasks consumer `Ready()`.
  dispatcher.RunToCompletion();
  // DOCSTAG: [pw_async2-examples-inline-async-queue-with-coro-run]

  return 0;
}

}  // namespace examples

namespace {

// Important: this test is only for verifying the example code above executes,
// and is not about verifying correctness. See
// https://pigweed.dev/contributing/docs/examples.html.

TEST(ExampleTests, InlineAsyncQueueWithCoro) {
  using namespace examples;

  main();

  EXPECT_EQ(examples::received_by_consumer.size(), 9U);
  EXPECT_TRUE(std::equal(kProducerExampleData.begin(),
                         kProducerExampleData.end(),
                         received_by_consumer.begin()));
  EXPECT_EQ(received_by_consumer[8U], kTerminal);
}

}  // namespace
