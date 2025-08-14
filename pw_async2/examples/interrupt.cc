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

// This example demonstrates a common use case for pw_async2: interacting with
// hardware that uses interrupts. It creates a fake UART device with an
// asynchronous reading interface and a separate thread that simulates hardware
// interrupts.

#include <termios.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_containers/inline_queue.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace {

// DOCSTAG: [pw_async2-examples-interrupt-uart]
// A fake UART device that provides an asynchronous byte reading interface.
class FakeUart {
 public:
  // Asynchronously reads a single byte from the UART.
  //
  // If a byte is available in the receive queue, it returns `Ready(byte)`.
  // If another task is already waiting for a byte, it returns
  // `Ready(Status::Unavailable())`.
  // Otherwise, it returns `Pending` and arranges for the task to be woken up
  // when a byte arrives.
  pw::async2::PollResult<char> ReadByte(pw::async2::Context& cx) {
    // Blocking inside an async function is generally an anti-pattern because it
    // prevents the single-threaded dispatcher from making progress on other
    // tasks. However, using `pw::sync::InterruptSpinLock` here is acceptable
    // due to the short-running nature of the ISR.
    std::lock_guard lock(lock_);

    // Check if the UART has been put into a failure state.
    PW_TRY(status_);

    // If a byte is already in the queue, return it immediately.
    if (!rx_queue_.empty()) {
      char byte = rx_queue_.front();
      rx_queue_.pop();
      return byte;
    }

    // If the queue is empty, the operation can't complete yet. Arrange for the
    // task to be woken up later.
    // `PW_ASYNC_TRY_STORE_WAKER` stores a waker from the current task's
    // context. If another task's waker is already stored, it returns false.
    if (PW_ASYNC_TRY_STORE_WAKER(cx, waker_, "Waiting for a byte from UART")) {
      return pw::async2::Pending();
    }

    // Another task is already waiting for a byte.
    return pw::Status::Unavailable();
  }

  // Simulates a hardware interrupt that receives a character.
  // This method is safe to call from an interrupt handler.
  void HandleReceiveInterrupt() {
    std::lock_guard lock(lock_);
    if (rx_queue_.full()) {
      // Buffer is full, drop the character.
      PW_LOG_WARN("UART RX buffer full, dropping character.");
      return;
    }

    // Generate a random lowercase letter to simulate receiving data.
    char c = 'a' + (std::rand() % 26);
    rx_queue_.push(c);

    // Wake any task that is waiting for the data. Waking an empty waker is a
    // no-op, so this can be called unconditionally.
    std::move(waker_).Wake();
  }

  // Puts the UART into a terminated state.
  void set_status(pw::Status status) {
    std::lock_guard lock(lock_);
    status_ = status;
    // Wake up any pending task so it can observe the status change and exit.
    std::move(waker_).Wake();
  }

 private:
  pw::sync::InterruptSpinLock lock_;
  pw::InlineQueue<char, 16> rx_queue_ PW_GUARDED_BY(lock_);
  pw::async2::Waker waker_;
  pw::Status status_;
};
// DOCSTAG: [pw_async2-examples-interrupt-uart]

FakeUart uart;

void SigintHandler(int /*signum*/) {
  std::printf("\r\033[K");  // Clear ^C from the terminal.
  uart.set_status(pw::Status::Cancelled());
}

}  // namespace

int main() {
  std::srand(static_cast<unsigned>(std::time(nullptr)));
  std::signal(SIGINT, SigintHandler);

  termios before;
  termios config;

  tcgetattr(STDIN_FILENO, &before);
  config = before;
  config.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
  tcsetattr(STDIN_FILENO, TCSANOW, &config);

  // DOCSTAG: [pw_async2-examples-interrupt-reader]
  pw::async2::Dispatcher dispatcher;

  // Create a task that reads from the UART in a loop.
  pw::async2::PendFuncTask reader_task(
      [](pw::async2::Context& cx) -> pw::async2::Poll<> {
        while (true) {
          // Poll `ReadByte` until it returns a `Ready`, then assign it to
          // `result`.
          PW_TRY_READY_ASSIGN(pw::Result<char> result, uart.ReadByte(cx));

          if (!result.ok()) {
            PW_LOG_ERROR("UART read failed: %s", result.status().str());
            break;
          }

          PW_LOG_INFO("Received: %c", *result);
        }

        return pw::async2::Ready();
      });

  // Post the task to the dispatcher to schedule it for execution.
  dispatcher.Post(reader_task);
  // DOCSTAG: [pw_async2-examples-interrupt-reader]

  std::thread interrupt_thread([] {
    while (true) {
      char c;
      read(STDIN_FILENO, &c, 1);
      if (c == ' ') {
        uart.HandleReceiveInterrupt();
      }
    }
  });
  interrupt_thread.detach();

  PW_LOG_INFO(
      "Fake UART initialized. Press spacebar to simulate receiving a "
      "character.");
  PW_LOG_INFO("Press Ctrl+C to exit.");

  // Run the dispatcher until all registered tasks terminate.
  dispatcher.RunToCompletion();

  tcsetattr(STDIN_FILENO, TCSANOW, &before);
  return 0;
}
