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

#pragma once

#include <atomic>
#include <memory>

#include "pw_digital_io_linux/internal/owned_fd.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_thread/thread_core.h"

namespace pw::digital_io {

// LinuxGpioNotifier waits for interrupts from a set of GPIO lines. The notifier
// is able to listen for interrupts across multiple GPIO chips, and multiple
// notifiers are able to listen for interrupts on different lines from one chip.
//
// Most applications will have one notifier running on a high thread priority.
// It is expected that the interrupt handlers are light-weight and will not
// block the notification thread. However, multiple notifiers can be created,
// potentially with different thread priorities.
//
// All methods of this class are thread-safe, and they can be called directly
// from within the interrupt handler itself.
//
class LinuxGpioNotifier : public pw::thread::ThreadCore {
  using OwnedFd = internal::OwnedFd;

 public:
  // Handler for GPIO line events.
  class Handler {
   public:
    virtual ~Handler() = default;

    // Handle events that occurred on this line. Any unhandled events will cause
    // the handler to be invoked again.
    virtual void HandleEvents() = 0;
  };

  // Create a new notifier or return status Internal on unexpected error.
  static pw::Result<std::shared_ptr<LinuxGpioNotifier>> Create();

  ~LinuxGpioNotifier() override;

  LinuxGpioNotifier(const LinuxGpioNotifier&) = delete;
  LinuxGpioNotifier& operator=(const LinuxGpioNotifier&) = delete;

  // Register a file descriptor to listen for notifications on. Invoke the
  // given handler when there are any events on the line.
  //
  // The handler must remain valid until UnregisterLine is called.
  //
  // Returns:
  //
  //  - OK: the descriptor was added to the epoll instance
  //  - InvalidArgument: the fd is not a valid file descriptor
  //  - FailedPrecondition: the descriptor is already registered
  //  - ResourceExhausted: epoll instance reached the limit imposed by
  //    /proc/sys/fs/epoll/max_user_watches
  //  - Internal: an unexpected error occurred
  //
  pw::Status RegisterLine(int fd, Handler& handler);

  // Unregister a file descriptor. No-op if the descriptor is not registered.
  //
  // Returns:
  //
  //  OK: the descriptor was unregistered, or it was not registered.
  //  InvalidArgument: the fd is not a valid file descriptor
  //  Internal: an unexpected error occurred
  //
  pw::Status UnregisterLine(int fd);

  // Cancels any pending wait for events.
  //
  // This causes a blocking WaitForEvents() call to return Cancelled.
  // It also causes Run() to return, and if this notifier is used with
  // a Thread, that thread will exit.
  //
  // This method is only intended to be used in tests.
  //
  void CancelWait();

  // Synchronously wait for events across all registered lines.
  //
  // Args:
  //
  //   timeout_ms: Timeout, in milliseconds, to wait.
  //       0 means don't wait at all (nonblocking).
  //       -1 means wait forever.
  //
  // Returns:
  //
  //  Ok: At least one event was handled. The result contains the number of
  //      events handled.
  //  Cancelled: Wait was cancelled due to CancelWait() call.
  //  DeadlineExceeded: The timeout expired before any events were consumed.
  //  Internal: An unexpected error occurred.
  pw::Result<unsigned int> WaitForEvents(int timeout_ms = -1);

  // The main entry point for the notification thread.
  //
  // This will run forever until CancelWait() is called.
  //
  // This is public so callers can choose to synchronously handle notifications
  // themselves without using a thread.
  void Run() override;

 private:
  // Construct a notifier using the provided epoll fd and event fd.
  // The event fd must be pre-registered with the epoll fd.
  LinuxGpioNotifier(OwnedFd&& epoll_fd, OwnedFd&& event_fd)
      : epoll_fd_(std::move(epoll_fd)), cancel_event_fd_(std::move(event_fd)) {}

  // The epoll file descriptor that lists the line descriptors to wait for.
  // This descriptor is initialized by Init(). After initialization, the
  // descriptor can be manipulated with epoll_* functions without any additional
  // synchronization.
  OwnedFd epoll_fd_;

  // An eventfd that is used to signal cancellation to the thread waiting on
  // notification.
  OwnedFd cancel_event_fd_;

  // An atomic counter of registered handles, which is used purely for
  // diagnostic purposes to detect incorrect usage of the API. This is necessary
  // because we can't inspect the epoll descriptor set. Note that atomics are
  // safe to use on cores that are capable of running Linux.
  std::atomic_int registered_line_count_ = 0;
};

}  // namespace pw::digital_io
