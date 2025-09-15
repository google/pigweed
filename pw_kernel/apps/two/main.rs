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
use userspace::entry;
use userspace::syscall::{self, Signals};
use userspace::time::Instant;

const IPC_HANDLE: u32 = 0;

fn handle_uppercase_ipcs() -> Result<()> {
    loop {
        // Wait for an IPC to come in.
        syscall::object_wait(IPC_HANDLE, Signals::READABLE, Instant::MAX)?;

        // Read the payload.
        let mut buffer = [0u8; size_of::<char>()];
        let len = syscall::channel_read(IPC_HANDLE, 0, &mut buffer)?;
        if len != size_of::<char>() {
            return Err(Error::OutOfRange);
        };

        // Convert the payload to a character and make it uppercase.
        let Some(c) = char::from_u32(u32::from_ne_bytes(buffer)) else {
            return Err(Error::InvalidArgument);
        };
        let c = c.to_ascii_uppercase();

        // Respond to the IPC with the uppercase character.
        let mut response_buffer = [0u8; size_of::<char>()];
        c.encode_utf8(&mut response_buffer);
        syscall::channel_respond(IPC_HANDLE, &response_buffer)?;
    }
}

#[entry]
fn entry() -> ! {
    if let Err(e) = handle_uppercase_ipcs() {
        // On error, log that it occurred and, since this is written as a test,
        // shut down the system with the error code.
        let _ = syscall::debug_putc('!');
        let _ = syscall::debug_shutdown(Err(e));
    }

    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
