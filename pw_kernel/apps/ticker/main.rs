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

use syscall_user::*;
use userspace::entry;

#[entry]
fn entry() -> ! {
    for i in 0..4 {
        const OBJECT_HANDLE: u32 = 0x0;
        const SIGNAL_MASK: u32 = 0x1;
        const DEADLINE: u64 = u64::MAX;

        let _ = SysCall::object_wait(OBJECT_HANDLE, SIGNAL_MASK, DEADLINE);
        let _ = SysCall::debug_putc(u32::from('0') + i);
        // On the third successful wait on tick, exit successfully.
        if i == 2 {
            let _ = SysCall::debug_shutdown(0);
        }
    }

    // It's an error if we end here, as we should have exited on the
    // third iteration.
    let _ = SysCall::debug_shutdown(1);
    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
