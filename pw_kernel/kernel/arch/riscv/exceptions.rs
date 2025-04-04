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
use core::arch::naked_asm;

use log_if::debug_if;
use pw_log::info;

use crate::arch::riscv::regs::{Cause, Exception, Interrupt, MCause, MCauseVal};
use crate::arch::riscv::timer;
use crate::syscall::raw_handle_syscall;

const LOG_EXCEPTIONS: bool = false;

#[inline(never)]
fn dump_exception_frame(frame: &TrapFrame) {
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
    info!("mstatus {:#010x}", frame.status as u32);
    info!("mcause {:#010x}", MCause::read().0 as u32);
    info!("epc {:#010x}", frame.epc as u32);
}

// Pulls arguments out of the trap frame and calls the arch-independent syscall
// handler.
fn handle_ecall(frame: &mut TrapFrame) {
    let ret_val = raw_handle_syscall(
        (*frame).t0 as u16,
        (*frame).a0 as usize,
        (*frame).a1 as usize,
        (*frame).a2 as usize,
        (*frame).a3 as usize,
    );
    (*frame).a0 = (ret_val as u64) as usize;
    (*frame).a1 = (ret_val as u64 >> 32) as usize;

    // ECALL exceptions do not "retire the instruction" requiring the advancing
    // of the PC past the ECALL instruction.  ECALLs are encoded as 4 byte
    // instructions.
    frame.epc += 4;
}

fn custom_exception_handler(exception: Exception, mepc: usize, frame: &mut TrapFrame) {
    // For now, always dump the exception we've received and halt.
    debug_if!(
        LOG_EXCEPTIONS,
        "Exception exception {:x} mepc {:x}",
        exception as usize,
        mepc as usize,
    );

    match exception {
        Exception::EnvironmentCallFromMMode => {
            handle_ecall(frame);
        }
        _ => {
            dump_exception_frame(frame);
            pw_assert::panic!("unhandled exception {}", exception as usize);
        }
    };
}

// Default interrupt handler
unsafe fn custom_interrupt_handler(interrupt: Interrupt, mepc: usize, frame: &TrapFrame) {
    debug_if!(
        LOG_EXCEPTIONS,
        "Interrupt interrupt {:x} mepc {:x}, trap_frame {:x}, mstatus {:x}",
        interrupt as usize,
        mepc as usize,
        core::ptr::addr_of!(frame) as usize,
        frame.status as usize,
    );

    match interrupt {
        Interrupt::MachineTimer => {
            timer::mtimer_tick();
        }
        _ => {
            pw_assert::panic!("unhandled interrupt {}", interrupt as usize);
        }
    }
}

#[no_mangle]
#[link_section = ".trap.rust"]
pub unsafe extern "C" fn trap_handler(mcause: MCauseVal, mepc: usize, frame: &mut TrapFrame) {
    match mcause.cause() {
        Cause::Interrupt(interrupt) => custom_interrupt_handler(interrupt, mepc, frame),
        Cause::Exception(exception) => custom_exception_handler(exception, mepc, frame),
    }
}

#[repr(C)]
pub struct TrapFrame {
    // Note: offsets are for 32bit sized usize
    epc: usize,    // 0x00
    status: usize, // 0x04
    ra: usize,     // 0x08

    a0: usize, // 0x0c
    a1: usize, // 0x10
    a2: usize, // 0x14
    a3: usize, // 0x18
    a4: usize, // 0x1c
    a5: usize, // 0x20
    a6: usize, // 0x24
    a7: usize, // 0x28

    t0: usize, // 0x2c
    t1: usize, // 0x30
    t2: usize, // 0x34
    t3: usize, // 0x38
    t4: usize, // 0x3c
    t5: usize, // 0x40
    t6: usize, // 0x44

    // These are only set when handling a trap from user space.
    tp: usize, // 0x48
    gp: usize, // 0x4c
    sp: usize, // 0x50

    // Per RISC-V calling convention, stacks must always be 16 byte aligned.
    pad: [usize; 3], // 0x54-0x5f
}

const _: () = assert!(core::mem::size_of::<TrapFrame>() == 0x60);

#[naked]
#[no_mangle]
#[link_section = ".trap"]
unsafe extern "C" fn _start_trap() {
    naked_asm!(
        "
        // TODO - konkers: swap stack pointer into mscratch and check for user space.
        // For now, use the same stack we came in on since we are not implementing protection.
        addi  sp, sp, -0x60

        // Note: All stack math here assumes 32bit word sizes.
        sw    t6, 0x44(sp)
        sw    t5, 0x40(sp)
        sw    t4, 0x3c(sp)
        sw    t3, 0x38(sp)
        sw    t2, 0x34(sp)
        sw    t1, 0x30(sp)
        sw    t0, 0x2c(sp)

        sw    a7, 0x28(sp)
        sw    a6, 0x24(sp)
        sw    a5, 0x20(sp)
        sw    a4, 0x1c(sp)
        sw    a3, 0x18(sp)
        sw    a2, 0x14(sp)
        sw    a1, 0x10(sp)
        sw    a0, 0x0c(sp)

        sw    ra, 0x08(sp)
        csrr  t0, mstatus
        sw    t0, 0x04(sp)

        csrr  a0, mcause
        csrr  a1, mepc
        sw    a1, 0x00(sp)

        mv    a2, sp

        // Call trap_handler with the following arguments:
        // a0: mcause
        // a1: mepc
        // a2: &TrapFrame
        call  trap_handler

        lw    t0, 0x00(sp)
        lw    t1, 0x04(sp)
        csrw  mepc, t0
        csrw  mstatus, t1

        lw    ra, 0x08(sp)

        lw    a0, 0x0c(sp)
        lw    a1, 0x10(sp)
        lw    a2, 0x14(sp)
        lw    a3, 0x18(sp)
        lw    a4, 0x1c(sp)
        lw    a5, 0x20(sp)
        lw    a6, 0x24(sp)
        lw    a7, 0x28(sp)

        lw    t0, 0x2c(sp)
        lw    t1, 0x30(sp)
        lw    t2, 0x34(sp)
        lw    t3, 0x38(sp)
        lw    t4, 0x3c(sp)
        lw    t5, 0x40(sp)
        lw    t6, 0x44(sp)

        addi  sp, sp, 0x60

        mret
    "
    );
}
