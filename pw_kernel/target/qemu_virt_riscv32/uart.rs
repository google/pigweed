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

use bitflags::bitflags;
use circular_buffer::CircularBuffer;
use kernel::sync::spinlock::SpinLock;
use pw_status::Result;

const LOG_UART: bool = false;

// The main uart for QEMU riscv32 virt is a simple 16550 clone at 0x1000_0000
pub const IRQ_UART0: u32 = 10;
const BASE_ADDR: usize = 0x1000_0000;
const IER_OFFSET: usize = 1;
const MCR_OFFSET: usize = 4;
const LSR_OFFSET: usize = 5;

bitflags! {
    #[derive(Copy, Clone)]
    pub struct InterruptEnableRegister: u8 {
        const RECEIVED_DATA_AVAILABLE = 1 << 0;
    }
}

bitflags! {
    #[derive(Copy, Clone)]
    pub struct ModemControlRegister: u8 {
        const LOOPBACK_ENABLE = 1 << 4;
    }
}

bitflags! {
    #[derive(Copy, Clone)]
    pub struct LineStatusRegister: u8 {
        const DATA_READY = 1 << 0;
        const THR_EMPTY = 1 << 5;
    }
}

const BUFFER_SIZE: usize = 128;
static BUFFER: SpinLock<arch_riscv::Arch, CircularBuffer<u8, BUFFER_SIZE>> =
    SpinLock::new(CircularBuffer::new());

pub struct Uart {}

impl Uart {
    pub fn enable_loopback(&self) {
        let mcr_ptr = ptr::with_exposed_provenance_mut::<u8>(BASE_ADDR + MCR_OFFSET);
        unsafe {
            let mut mcr_value = ModemControlRegister::from_bits_retain(mcr_ptr.read_volatile());
            mcr_value.insert(ModemControlRegister::LOOPBACK_ENABLE);
            mcr_ptr.write_volatile(mcr_value.bits());
        }
    }

    pub fn read(&self) -> Option<u8> {
        log_if::debug_if!(LOG_UART, "uart read");
        BUFFER.lock(arch_riscv::Arch).pop_front()
    }

    pub fn write(&self, value: u8) -> Result<()> {
        log_if::debug_if!(LOG_UART, "uart write: {}", value as u8);
        unsafe {
            let lsr_ptr = core::ptr::with_exposed_provenance::<u8>(BASE_ADDR + LSR_OFFSET);
            while !LineStatusRegister::from_bits_truncate(lsr_ptr.read_volatile())
                .contains(LineStatusRegister::THR_EMPTY)
            {}
            let ptr: *mut u8 = core::ptr::with_exposed_provenance_mut::<u8>(BASE_ADDR);
            ptr.write_volatile(value);
            log_if::debug_if!(LOG_UART, "done");
        }
        Ok(())
    }
}

pub fn init() {
    let ier_ptr = ptr::with_exposed_provenance_mut::<u8>(BASE_ADDR + IER_OFFSET);
    unsafe {
        ier_ptr.write_volatile(InterruptEnableRegister::RECEIVED_DATA_AVAILABLE.bits());
    }
}

pub fn uart_interrupt_handler() {
    log_if::debug_if!(LOG_UART, "uart_interrupt_handler");
    unsafe {
        let lsr_ptr = core::ptr::with_exposed_provenance::<u8>(BASE_ADDR + LSR_OFFSET);
        while LineStatusRegister::from_bits_truncate(lsr_ptr.read_volatile())
            .contains(LineStatusRegister::DATA_READY)
        {
            let ptr = core::ptr::with_exposed_provenance::<u8>(BASE_ADDR);
            let value = ptr.read_volatile();
            log_if::debug_if!(LOG_UART, "data ready: {}", value as u8);
            let _ = BUFFER.lock(arch_riscv::Arch).push_back(value);
        }
    }
}
