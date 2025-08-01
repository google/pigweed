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

#[cfg(feature = "arch_arm_cortex_m")]
use cortex_m_semihosting::hio::hstdout;
use pw_status::{Error, Result};
#[cfg(feature = "arch_riscv")]
use riscv_semihosting::hio::hstdout;

#[unsafe(no_mangle)]
pub fn console_backend_write_all(buf: &[u8]) -> Result<()> {
    let mut stdout = hstdout().map_err(|_| Error::Unavailable)?;
    stdout.write_all(buf).map_err(|_| Error::DataLoss)?;
    Ok(())
}
