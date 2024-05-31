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

#include <chrono>

#include "pw_chrono/system_clock.h"
#include "pw_digital_io_linux/digital_io.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

using namespace std::chrono_literals;

using pw::digital_io::InterruptTrigger;
using pw::digital_io::LinuxDigitalIoChip;
using pw::digital_io::LinuxGpioNotifier;
using pw::digital_io::LinuxInputConfig;
using pw::digital_io::Polarity;
using pw::digital_io::State;
using pw::thread::Thread;

pw::Status InterruptExample() {
  // Open handle to chip.
  PW_TRY_ASSIGN(auto chip, LinuxDigitalIoChip::Open("/dev/gpiochip0"));

  // Create a notifier to deliver interrupts to the line.
  PW_TRY_ASSIGN(auto notifier, LinuxGpioNotifier::Create());

  // Configure input line.
  LinuxInputConfig config(
      /* index= */ 5,
      /* polarity= */ Polarity::kActiveHigh);
  PW_TRY_ASSIGN(auto input, chip.GetInterruptLine(config, notifier));

  // Configure the interrupt handler.
  auto handler = [](State state) {
    PW_LOG_DEBUG("Interrupt handler fired with state=%s",
                 state == State::kActive ? "active" : "inactive");
  };
  PW_TRY(input.SetInterruptHandler(InterruptTrigger::kActivatingEdge, handler));
  PW_TRY(input.EnableInterruptHandler());
  PW_TRY(input.Enable());

  // There are several different ways to deal with events:

  // Option A: Wait once for events.
  PW_TRY(notifier->WaitForEvents(0));    // Non-blocking.
  PW_TRY(notifier->WaitForEvents(500));  // Block for 500 milliseconds.
  PW_TRY(notifier->WaitForEvents(-1));   // Block indefinitely.

  // Option B: Handle events synchronously, blocking forever.
  notifier->Run();

  // Option C: Handle events in a separate thread.
  Thread notifier_thread(pw::thread::stl::Options(), *notifier);
  pw::this_thread::sleep_for(30s);
  notifier->CancelWait();
  notifier_thread.join();

  return pw::OkStatus();
}
