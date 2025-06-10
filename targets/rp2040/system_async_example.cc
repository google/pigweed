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

#define PW_LOG_MODULE_NAME "pw_system_async"

#include "pico/stdlib.h"
#include "pw_channel/rp2_stdio_channel.h"
#include "pw_multibuf/simple_allocator_for_test.h"
#include "pw_system/system.h"

int main() {
  // PICO_SDK Inits
  stdio_init_all();
  setup_default_uart();
  stdio_usb_init();

  static pw::multibuf::test::SimpleAllocatorForTest<4096, 4096> mb_alloc;
  pw::SystemStart(pw::channel::Rp2StdioChannelInit(mb_alloc, mb_alloc));
  PW_UNREACHABLE;
}
