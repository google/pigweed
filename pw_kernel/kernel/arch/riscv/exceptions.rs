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
#![allow(non_snake_case)]

use crate::arch::riscv::timer;
use log_if::debug_if;
use pw_log::info;
use riscv::interrupt::*;
use riscv::register::*;

const LOG_EXCEPTIONS: bool = false;

// TODO: move away from riscv-rt crate and implement the low level exception handlers.
// This will allow us to implement user and machine/supervisor mode trap
// differentiation and push a full frame on some exceptions for better debugging.

#[inline(never)]
fn dump_exception_frame(frame: &riscv_rt::TrapFrame) {
    info!(
        "Exception frame {:#08x}:",
        core::ptr::addr_of!(frame) as usize
    );

    info!(
        "ra  {:#010x} t0 {:#010x} t1  {:#010x} t2  {:#010x}",
        frame.ra as u32, frame.t0 as u32, frame.t1 as u32, frame.t2 as u32
    );
    info!(
        "t3  {:#010x} t4 {:#010x} t5  {:#010x} t6  {:#010x}",
        frame.t3 as u32, frame.t4 as u32, frame.t5 as u32, frame.t6 as u32
    );
    info!(
        "a0  {:#010x} a1 {:#010x} a2  {:#010x} a3  {:#010x}",
        frame.a0 as u32, frame.a1 as u32, frame.a2 as u32, frame.a3 as u32
    );
    info!(
        "a4  {:#010x} a5 {:#010x} a6  {:#010x} a7  {:#010x}",
        frame.a4 as u32, frame.a5 as u32, frame.a6 as u32, frame.a7 as u32
    );
}

// Default exception handler
#[export_name = "ExceptionHandler"]
fn custom_exception_handler(trap_frame: &riscv_rt::TrapFrame) -> ! {
    let mcause = mcause::read();
    let mepc = mepc::read();

    // For now, always dump the exception we've received and halt.
    info!(
        "Exception mcause {:x} mepc {:x}",
        mcause.bits() as usize,
        mepc as usize
    );
    dump_exception_frame(trap_frame);
    loop {
        riscv::asm::wfi();
    }
}

// Default interrupt handler
#[export_name = "DefaultHandler"]
unsafe fn custom_interrupt_handler() {
    debug_if!(
        LOG_EXCEPTIONS,
        "Interrupt mcause {:x} mepc {:x}",
        mcause::read().bits() as usize,
        mepc::read() as usize
    );

    let mcause = mcause::read();
    if mcause.is_interrupt() {
        match Interrupt::from_number(mcause.code()) {
            Ok(Interrupt::MachineTimer) => {
                timer::mtimer_tick();
            }
            _ => {
                panic!("unhandled interrupt {}", mcause.code());
            }
        }
    } else {
        panic!("exception in interrupt handler!");
    }
}
