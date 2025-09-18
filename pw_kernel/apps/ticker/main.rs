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
#![no_main]
#![no_std]

use app_ticker::handle;
use pw_status::Error;
use userspace::entry;
use userspace::syscall::{self, Signals};
use userspace::time::Instant;

#[entry]
fn entry() -> ! {
    for c in '0'..'4' {
        let _ = syscall::object_wait(handle::TICKER, Signals::READABLE, Instant::MAX);
        let _ = syscall::debug_putc(c);
        // On the third successful wait on tick, exit successfully.
        if c == '2' {
            let _ = syscall::debug_shutdown(Ok(()));
        }
    }

    // It's an error if we end here, as we should have exited on the
    // third iteration.
    let _ = syscall::debug_shutdown(Err(Error::Unknown));
    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
