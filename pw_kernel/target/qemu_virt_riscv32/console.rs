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

use core::ptr;

use kernel::sync::spinlock::SpinLock;
use pw_status::Result;

// The main uart for QEMU riscv32 virt is a simple 16550 clone at 0x1000_0000
const BASE_ADDR: usize = 0x1000_0000;

struct Uart {
    base_addr: usize,
}

impl Uart {
    fn write_all(&self, buf: &[u8]) -> Result<()> {
        unsafe {
            for c in buf.iter() {
                // Simply write the character to the fifo, assuming it
                // will never back up. This is reasonable for a first
                // implementation, but it is possible for qemu to eventually
                // fill its buffers.
                let ptr = ptr::with_exposed_provenance_mut::<u8>(self.base_addr);
                ptr::write_volatile(ptr, *c);
            }
            Ok(())
        }
    }
}

static UART: SpinLock<arch_riscv::BareSpinLock, Uart> = SpinLock::new(Uart {
    base_addr: BASE_ADDR,
});

#[no_mangle]
pub fn console_backend_write_all(buf: &[u8]) -> Result<()> {
    let uart = UART.lock();
    uart.write_all(buf)
}
