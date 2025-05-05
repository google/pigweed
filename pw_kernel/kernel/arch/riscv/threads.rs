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
use crate::scheduler::{self, thread::Stack, SchedulerState};
use crate::sync::spinlock::SpinLockGuard;
use core::arch::naked_asm;
use core::mem::{self, MaybeUninit};
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

impl ArchThreadState {
    #[inline(never)]
    fn initialize_frame(
        &mut self,
        kernel_stack: Stack,
        trampoline: extern "C" fn(),
        initial_sp: usize,
        initial_function: extern "C" fn(usize, usize),
        (arg0, arg1): (usize, usize),
    ) {
        let frame = Stack::aligned_stack_allocation(
            kernel_stack.initial_sp(8),
            size_of::<ContextSwitchFrame>(),
            8,
        );
        let frame: *mut ContextSwitchFrame = frame as *mut ContextSwitchFrame;
        unsafe { (*frame) = mem::zeroed() };

        unsafe {
            // Clear the stack and set up the exception frame such that it would
            // return to the function passed in with arg0 and arg1 passed in the
            // first two argument slots.
            (*frame).ra = trampoline as usize;
            (*frame).s0 = initial_function as usize;
            (*frame).s1 = arg0;
            (*frame).s2 = arg1;
            (*frame).s5 = initial_sp;
        }

        self.frame = frame
    }
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

    fn initialize_kernel_frame(
        &mut self,
        kernel_stack: Stack,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    ) {
        self.initialize_frame(kernel_stack, asm_trampoline, 0x0, initial_function, args);
    }

    #[cfg(feature = "user_space")]
    fn initialize_user_frame(
        &mut self,
        kernel_stack: Stack,
        initial_sp: *mut MaybeUninit<u8>,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    ) {
        self.initialize_frame(
            kernel_stack,
            asm_user_trampoline,
            initial_sp as usize,
            initial_function,
            args,
        );
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
// pass the initial function and arguments via two of the saved s registers.
#[no_mangle]
#[naked]
extern "C" fn asm_user_trampoline() {
    unsafe {
        naked_asm!(
            "
                // Store the kernel stack pointer in mscratch.
                csrw    mscratch, sp

                // Set initial SP as pass in by `initialize_frame()`
                mv      sp, s5

                // Set args for function call
                mv      a0, s0
                mv      a1, s1
                mv      a2, s2

                // TODO: mret into non-privileged mode
                tail    trampoline
            "
        )
    }
}

// Since the context switch frame does not contain the function arg registers,
// pass the initial function and arguments via two of the saved s registers.
#[no_mangle]
#[naked]
extern "C" fn asm_trampoline() {
    unsafe {
        naked_asm!(
            "
                // Zero out mscratch to signify that this is a kernel thread.
                csrw    mscratch, zero
                mv a0, s0
                mv a1, s1
                mv a2, s2
                tail trampoline
            "
        )
    }
}

#[allow(unused)]
#[no_mangle]
extern "C" fn trampoline(initial_function: extern "C" fn(usize, usize), arg0: usize, arg1: usize) {
    debug_if!(
        LOG_THREAD_CREATE,
        "riscv trampoline: initial function {:#x} arg0 {:#x} arg1 {:#x}",
        initial_function as usize,
        arg0 as usize,
        arg1 as usize
    );

    // Enable interrupts
    Arch::enable_interrupts();

    // TODO: figure out how to drop the scheduler lock here?

    // Call the actual initial function of the thread.
    initial_function(arg0, arg1);

    // Get a pointer to the current thread and call exit.
    // Note: must let the scope of the lock guard close,
    // since exit_thread() does not return.
    scheduler::exit_thread();

    // Does not reach.
}
