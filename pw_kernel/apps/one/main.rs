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

use app_one::handle;
use pw_status::{Error, Result, StatusCode};
use userspace::entry;
use userspace::syscall::{self, Signals};
use userspace::time::Instant;

fn test_uppercase_ipcs() -> Result<()> {
    pw_log::info!("Ipc test starting");
    for c in 'a'..='z' {
        let mut send_buf = [0u8; size_of::<char>()];
        let mut recv_buf = [0u8; size_of::<char>()];

        // Encode the character into `send_buf` and send it over to the handler.
        c.encode_utf8(&mut send_buf);
        let len: usize =
            syscall::channel_transact(handle::IPC, &send_buf, &mut recv_buf, Instant::MAX)?;

        // The handler side always sends 4 bytes to make up a full Rust `char`
        if len != size_of::<char>() {
            return Err(Error::OutOfRange);
        }

        // Log the response character
        let Ok(upper_c) = u32::from_ne_bytes(recv_buf).try_into() else {
            return Err(Error::InvalidArgument);
        };
        let upper_c: char = upper_c;

        pw_log::info!("sent {}, received {}", c as char, upper_c as char);

        // Verify that the remote side made the character uppercase.
        if upper_c != c.to_ascii_uppercase() {
            return Err(Error::Unknown);
        }
    }

    Ok(())
}

#[entry]
fn entry() -> ! {
    let ret = test_uppercase_ipcs();

    // Log that an error occurred so that the app that caused the shutdown is logged.
    if ret.is_err() {
        pw_log::error!("Error {}", ret.status_code() as u32);
    }

    // Wait for as ticker event before shutting down the system.
    let _ = syscall::object_wait(handle::TICKER, Signals::READABLE, Instant::MAX);

    // Since this is written as a test, shut down with the return status from `main()`.
    let _ = syscall::debug_shutdown(ret);
    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
