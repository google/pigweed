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

use embedded_io::Write;
use kernel::sync::spinlock::SpinLock;
use pw_status::{Error, Result};
use rp235x_hal::gpio::bank0::{Gpio12, Gpio13};
use rp235x_hal::gpio::{FunctionUart, Pin, PullDown};
use rp235x_hal::pac::UART0;
use rp235x_hal::uart::{self, UartPeripheral};

pub type ConsoleUart = UartPeripheral<
    uart::Enabled,
    UART0,
    (
        Pin<Gpio12, FunctionUart, PullDown>,
        Pin<Gpio13, FunctionUart, PullDown>,
    ),
>;

static UART: SpinLock<Option<ConsoleUart>> = SpinLock::new(None);

#[no_mangle]
pub fn console_backend_write_all(buf: &[u8]) -> Result<()> {
    let mut uart = UART.lock();
    match &mut (*uart) {
        Some(uart) => uart.write_all(buf).map_err(|_| Error::Unknown),
        None => Ok(()),
    }
}

pub fn register_uart(val: ConsoleUart) {
    let mut uart = UART.lock();
    *uart = Some(val)
}
