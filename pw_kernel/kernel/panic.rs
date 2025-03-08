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

use core::panic::PanicInfo;

use pw_log::fatal;

use crate::arch::{Arch, ArchInterface};

// Temporary panic handler.
//
// This is very useful for debugging but the Rust panic infrastructure is quite
// heavy weight and not appropriate for production.
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    let msg = info.message().as_str().unwrap_or("");
    if let Some(location) = info.location() {
        fatal!(
            "Panic at {}:{}: {}",
            location.file() as &str,
            location.line() as u32,
            msg as &str
        );
    } else {
        fatal!("Panic: {}", msg as &str);
    }

    <Arch as ArchInterface>::panic()
}
