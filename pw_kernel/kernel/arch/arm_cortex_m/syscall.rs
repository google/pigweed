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

use crate::syscall::raw_handle_syscall;

use super::exceptions::ExceptionFrame;

// Pulls arguments out of the exception frame and calls the arch-independent
// syscall handler.
//
// There is potential to put the call to `raw_handle_syscall()` directly into
// the SVCall handler to save some code space and execution time.

#[no_mangle]
unsafe extern "C" fn handle_svc(frame: &mut ExceptionFrame) {
    let ret_val = raw_handle_syscall(
        (*frame).r12 as u16,
        (*frame).r0 as usize,
        (*frame).r1 as usize,
        (*frame).r2 as usize,
        (*frame).r3 as usize,
    );
    (*frame).r0 = (ret_val as u64) as u32;
    (*frame).r1 = (ret_val as u64 >> 32) as u32;
}

/// Raw SVCall exception handler.
///
/// Handles both syscall entry as well as exit.  Since we want to allow syscalls
/// to block and be preempted, they are run in thread mode by returning from
/// this exception through a "fake" exception frame.  Syscall exit also uses
/// this exception to applying memory protection, setting the correct privilege
/// mode, and stack pointer atomically.
#[no_mangle]
#[naked]
pub unsafe extern "C" fn SVCall() -> ! {
    unsafe {
        #![allow(named_asm_labels)]
        naked_asm!(
            "
            // Cortex-M exception frame:
            // #28 - RETPSR
            // #24 - Return Address
            // #20 - LR (R14)
            // #16 - IP (R12)
            // #12 - R3
            // #08 - R2
            // #04 - R1
            // #00 - R0

            // Save return address
            ldr r2, [sp, #24]

            // Check for return from syscall by comparing PC of the exception
            // frame to the `svc 0` after handling the syscall below.
            ldr r1, =_arch_handle_svc_wrapper_return
            cmp r2, r1
            beq _arch_return_from_syscall

            // Push a replacement exception frame to the stack which is set to
            // return to from the exception the scv handler in kernel thread mode.
            mov r0, #0
            mov r1, #0
            ldr r2, =_arch_handle_svc_wrapper

            // RETPSR: T32 mode with all flags cleared and un-aligned stacks.
            ldr r3, =0x01000000

            push {{r0-r3}} // r12/ip, r14/lr, Return Address, PSR
            // r0-r3's values are not used by the wrapper so the previous values
            // are pushed again in a single instruction to save on code space
            push {{r0-r3}} // r0-r3

            // TODO:
            // * clear nPriv
            // * switch to Main (aka kernel) stack (modify PSEL in lr)

            bx lr

        _arch_handle_svc_wrapper:
            mov     r0, sp
            bl      handle_svc

            // Raise SVCall exception again to return to the syscall caller
            svc     0

        _arch_handle_svc_wrapper_return:
            // The `svc 0` above should return to the sycall caller instead of
            // this point so this instruction should never run.
            bkpt

        _arch_return_from_syscall:
            // drop this exception frame and return through the original frame
            add     sp, 32

            // TODO:
            // * conditionally set nPriv
            // * switch to correct stack for thread (modify PSEL in lr)
            bx lr
            "
        )
    }
}
