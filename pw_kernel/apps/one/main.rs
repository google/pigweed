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

use pw_tokenizer::tokenize_core_fmt_to_buffer;
use syscall_user::*;
use userspace::entry;

#[entry]
fn entry() -> ! {
    // populate the tokenized database
    let mut buffer = [0u8; 1024];
    let _ = tokenize_core_fmt_to_buffer!(&mut buffer, "App one tokenized string {}", 1 as i32);
    loop {
        for c in 'a'..'z' {
            const OBJECT_HANDLE: u32 = 0x0;
            const SIGNAL_MASK: u32 = 0x1;
            const DEADLINE: u64 = u64::MAX;

            let _ = SysCall::object_wait(OBJECT_HANDLE, SIGNAL_MASK, DEADLINE);
            let _ = SysCall::debug_putc(u32::from(c));
        }
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
