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

use crate::arch::{Arch, ArchInterface};
use crate::scheduler::{self, SchedulerState, Stack};
use crate::sync::spinlock::SpinLockGuard;
use core::arch::naked_asm;
use core::mem;
use log_if::debug_if;

const LOG_CONTEXT_SWITCH: bool = false;
const LOG_THREAD_CREATE: bool = false;

#[repr(C)]
struct ContextSwitchFrame {
    ra: usize,
    s0: usize,
    s1: usize,
    s2: usize,
    s3: usize,
    s4: usize,
    s5: usize,
    s6: usize,
    s7: usize,
    s8: usize,
    s9: usize,
    s10: usize,
    s11: usize,
}

pub struct ArchThreadState {
    frame: *mut ContextSwitchFrame,
}

impl super::super::ThreadState for ArchThreadState {
    fn new() -> Self {
        Self {
            frame: core::ptr::null_mut(),
        }
    }

    #[inline(never)]
    unsafe fn context_switch<'a>(
        sched_state: SpinLockGuard<'a, SchedulerState>,
        old_thread_state: *mut ArchThreadState,
        new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'a, SchedulerState> {
        debug_if!(
            LOG_CONTEXT_SWITCH,
            "context switch from frame {:#08x} to frame {:#08x}",
            (*old_thread_state).frame as usize,
            (*new_thread_state).frame as usize,
        );

        riscv_context_switch(&mut (*old_thread_state).frame, (*new_thread_state).frame);

        sched_state
    }

    #[inline(never)]
    fn initialize_frame(&mut self, stack: Stack, initial_function: fn(usize), arg0: usize) {
        // Calculate the first 8 byte aligned full exception frame from the top
        // of the thread's stack.
        let mut frame = stack.end().wrapping_sub(size_of::<ContextSwitchFrame>());
        let offset = frame.align_offset(8);
        if offset > 0 {
            frame = frame.wrapping_sub(8 - offset);
        }
        let frame: *mut ContextSwitchFrame = frame as *mut ContextSwitchFrame;

        // Clear the stack and set up the exception frame such that it would return to the
        // function passed in with arg0 passed in the first argument slot.
        unsafe {
            (*frame) = mem::zeroed();
            (*frame).ra = asm_trampoline as usize;
            (*frame).s0 = initial_function as usize;
            (*frame).s1 = arg0;
        }

        self.frame = frame
    }
}

#[no_mangle]
#[naked]
extern "C" fn riscv_context_switch(
    old_frame: *mut *mut ContextSwitchFrame,
    new_frame: *mut ContextSwitchFrame,
) {
    unsafe {
        naked_asm!(
            "
                // Push all of the s registers + ra onto the stack,
                // save the stack pointer into the old frame pointer.
                addi sp, sp, -52
                sw  ra, 0(sp)
                sw  s0, 4(sp)
                sw  s1, 8(sp)
                sw  s2, 12(sp)
                sw  s3, 16(sp)
                sw  s4, 20(sp)
                sw  s5, 24(sp)
                sw  s6, 28(sp)
                sw  s7, 32(sp)
                sw  s8, 36(sp)
                sw  s9, 40(sp)
                sw  s10, 44(sp)
                sw  s11, 48(sp)
                sw  sp, (a0)

                // Pop ra and s registers off the stack.
                lw  ra, 0(a1)
                lw  s0, 4(a1)
                lw  s1, 8(a1)
                lw  s2, 12(a1)
                lw  s3, 16(a1)
                lw  s4, 20(a1)
                lw  s5, 24(a1)
                lw  s6, 28(a1)
                lw  s7, 32(a1)
                lw  s8, 36(a1)
                lw  s9, 40(a1)
                lw  s10, 44(a1)
                lw  s11, 48(a1)
                addi sp, a1, 52

                ret
            "
        )
    }
}

// Since the context switch frame does not contain the function arg registers,
// pass the initial function and argument via one of the saved s register.
#[no_mangle]
#[naked]
extern "C" fn asm_trampoline() {
    unsafe {
        naked_asm!(
            "
                mv a0, s0
                mv a1, s1
                tail trampoline
            "
        )
    }
}

#[allow(unused)]
#[no_mangle]
fn trampoline(initial_function: fn(usize), arg0: usize) {
    debug_if!(
        LOG_THREAD_CREATE,
        "riscv trampoline: initial function {:#x} arg {:#x}",
        initial_function as usize,
        arg0 as usize
    );

    // Enable interrupts
    Arch::enable_interrupts();

    // TODO: figure out how to drop the scheduler lock here?

    // Call the actual initial function of the thread.
    initial_function(arg0);

    // Get a pointer to the current thread and call exit.
    // Note: must let the scope of the lock guard close,
    // since exit_thread() does not return.
    scheduler::exit_thread();

    // Does not reach.
}
