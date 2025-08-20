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

#include "hardware.h"

#include <cstdio>
#include <cstdlib>
#include <thread>

#include "pw_log/log.h"

namespace codelab {

void HardwareLoop() {
  while (true) {
    char c = static_cast<char>(std::getchar());
    if (c == 'c') {
      coin_inserted_isr();
    } else if (std::isdigit(c)) {
      key_press_isr(c - '0');
    } else if (c == 'q') {
      // This is a simple exit mechanism for the codelab.
      std::exit(0);
    }
  }
}

void HardwareInit() {
  std::thread hardware_thread(HardwareLoop);
  hardware_thread.detach();
}

}  // namespace codelab
