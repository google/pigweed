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
    for i in 0..20 {
        let _ = SysCall::debug_putc(u32::from('B') + i);
    }

    loop {
        // Access memory out side of our address space to demonstrate that
        // memory protection is functional.
        unsafe {
            let bad = core::ptr::with_exposed_provenance::<usize>(0x80100000_usize);
            let _ = bad.read_volatile();
        }
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
