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

use pw_status::{Error, Result, StatusCode};
use syscall_user::*;
use userspace::entry;

const TICKER_HANDLE: u32 = 0;
const IPC_HANDLE: u32 = 1;

const OBJECT_READABLE: u32 = 0x1;

#[expect(dead_code)]
const OBJECT_WRITEABLE: u32 = 0x2;

const MAX_DEADLINE: u64 = u64::MAX;

fn main() -> Result<()> {
    for c in 'a'..='z' {
        let mut send_buf = [0u8; 4];
        let mut recv_buf = [0u8; 4];

        // Encode the character into `send_buf` and send it over to the handler.
        c.encode_utf8(&mut send_buf);
        let len = SysCall::channel_transact(
            IPC_HANDLE,
            send_buf.as_mut_ptr(),
            4,
            recv_buf.as_mut_ptr(),
            4,
            MAX_DEADLINE,
        )?;

        // The handler side always sends 4 bytes to make up a full Rust `char`
        if len != 4 {
            return Err(Error::Unknown);
        }

        let response_val = u32::from_ne_bytes(recv_buf);

        // Log the response as a character.
        SysCall::debug_putc(response_val)?;

        let Some(upper_c) = char::from_u32(response_val) else {
            return Err(Error::Unknown);
        };

        if upper_c != c.to_ascii_uppercase() {
            return Err(Error::Unknown);
        }
    }

    Ok(())
}

#[entry]
fn entry() -> ! {
    let ret = main();

    if ret.is_err() {
        let _ = SysCall::debug_putc(u32::from('!'));
    }

    // Wait for as ticker event before shutting down the system.
    let _ = SysCall::object_wait(TICKER_HANDLE, OBJECT_READABLE, MAX_DEADLINE);

    let _ = SysCall::debug_shutdown(ret.status_code());
    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
