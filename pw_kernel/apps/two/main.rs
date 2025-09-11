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

use pw_status::{Error, Result};
use syscall_user::*;
use userspace::entry;

const IPC_HANDLE: u32 = 0;

const OBJECT_READABLE: u32 = 0x1;

const MAX_DEADLINE: u64 = u64::MAX;

fn main() -> Result<()> {
    loop {
        // Wait for an IPC to come in.
        SysCall::object_wait(IPC_HANDLE, OBJECT_READABLE, MAX_DEADLINE)?;

        // Read the payload.
        let mut buffer = [0u8; 4];
        let len = SysCall::channel_read(IPC_HANDLE, 0, buffer.as_mut_ptr(), 4)?;
        if len != 4 {
            return Err(Error::Unknown);
        };

        // Convert the payload to a character and make it upper case.
        let Some(c) = char::from_u32(u32::from_ne_bytes(buffer)) else {
            return Err(Error::Unknown);
        };
        let c = c.to_ascii_uppercase();

        // Respond to the IPC with the uppercase character.
        let mut response_buffer = [0u8; 4];
        c.encode_utf8(&mut response_buffer);
        SysCall::channel_respond(IPC_HANDLE, response_buffer.as_mut_ptr(), 4)?;
    }
}

#[entry]
fn entry() -> ! {
    if let Err(e) = main() {
        let _ = SysCall::debug_putc(u32::from('!'));
        let _ = SysCall::debug_shutdown(e as u32);
    }

    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
