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
#![no_std]

use embedded_io::{ErrorType, Write};
use pw_status::Result;

pub struct Console {}

unsafe extern "Rust" {
    fn console_backend_write(buf: &[u8]) -> Result<usize>;
    fn console_backend_flush() -> Result<()>;
}

impl ErrorType for Console {
    type Error = pw_status::Error;
}

impl Default for Console {
    fn default() -> Self {
        Self::new()
    }
}

impl Console {
    #[inline]
    pub const fn new() -> Self {
        Self {}
    }
}

impl Write for Console {
    #[inline]
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        unsafe { console_backend_write(buf) }
    }

    #[inline]
    fn flush(&mut self) -> Result<()> {
        unsafe { console_backend_flush() }
    }
}
