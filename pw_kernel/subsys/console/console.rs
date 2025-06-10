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

use core::fmt::{Error, Write};
use embedded_io::ErrorType;
use pw_status::Result;

pub struct Console {}

unsafe extern "Rust" {
    fn console_backend_write_all(buf: &[u8]) -> Result<()>;
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
    #[must_use]
    #[inline]
    pub const fn new() -> Self {
        Self {}
    }

    #[inline]
    pub fn write_all(&mut self, buf: &[u8]) -> Result<()> {
        unsafe { console_backend_write_all(buf) }
    }
}

impl Write for Console {
    #[inline]
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        self.write_all(s.as_bytes()).map_err(|_| Error)
    }
}
