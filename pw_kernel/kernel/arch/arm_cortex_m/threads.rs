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

use core::arch::{asm, naked_asm};
use core::mem;
use cortex_m::peripheral::SCB;

// use pw_log::info;

use crate::arch::arm_cortex_m::{exceptions::FullExceptionFrame, *};
use crate::scheduler::{self, SchedulerState, Stack, SCHEDULER_STATE};
use crate::sync::spinlock::SpinLockGuard;

// Remember the thread that the cpu is currently running off of.
// NOTE this may lag behind the Scheduler's notion of current_thread due to the way
// pendsv may have a queuing effect in particular contexts, notably when preempting
// from a higher priority interrupt.
// Cleared inside the pendsv handler when the context switch actually happens.
static mut ACTIVE_THREAD: *mut ArchThreadState = core::ptr::null_mut();
unsafe fn get_active_thread() -> *mut ArchThreadState {
    ACTIVE_THREAD
}
unsafe fn set_active_thread(t: *mut ArchThreadState) {
    ACTIVE_THREAD = t;
}

pub struct ArchThreadState {
    #[allow(dead_code)]
    frame: *mut FullExceptionFrame,
}

impl super::super::ThreadState for ArchThreadState {
    fn new() -> Self {
        Self {
            frame: core::ptr::null_mut(),
        }
    }

    unsafe fn context_switch<'a>(
        mut sched_state: SpinLockGuard<'a, SchedulerState>,
        old_thread_state: *mut ArchThreadState,
        new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'a, SchedulerState> {
        pw_assert::assert!(new_thread_state == sched_state.get_current_arch_thread_state());
        // TODO - konkers: Allow $expr to be tokenized.

        // info!(
        //     "context switch from thread {:#08x} to thread {:#08x}",
        //     old_thread.id(),
        //     new_thread.id()
        // );

        // Remember active_thread only if it wasn't already set and trigger
        // a pendsv only the first time
        unsafe {
            if get_active_thread() == core::ptr::null_mut() {
                set_active_thread(old_thread_state);

                // Queue a PendSV
                SCB::set_pendsv();
            }
        }

        // Slightly different path based on if we're already inside an interrupt handler or not.
        if !in_interrupt_handler() {
            // A trick to reenable interrupts temporarily. Drop the passed in
            // spinlock guard temporarily enough to reenable interrupts and then
            // acquire immediately afterwards.

            // TODO: make sure this always drops interrupts, may need to force a cpsid here.
            drop(sched_state);

            // PendSV should fire and a context switch will happen.

            // ==== TEMPORAL ANOMALY HERE ====
            //
            // The next line of code is only executed in this context after the
            // old thread is context switched back to.

            sched_state = SCHEDULER_STATE.lock();
        } else {
            // in interrupt context the pendsv should have already triggered it
            pw_assert::assert!(SCB::is_pendsv_pending());
        }
        sched_state
    }

    #[inline(never)]
    fn initialize_frame(&mut self, stack: Stack, initial_function: fn(usize), arg0: usize) {
        // Calculate the first 8 byte aligned full exception frame from the top
        // of the thread's stack.
        let mut frame = stack.end().wrapping_sub(size_of::<FullExceptionFrame>());
        let offset = frame.align_offset(8);
        if offset > 0 {
            frame = frame.wrapping_sub(8 - offset);
        }
        let frame: *mut FullExceptionFrame = frame as *mut FullExceptionFrame;

        // Clear the stack and set up the exception frame such that it would return to the
        // function passed in with arg0 passed in the first argument slot.
        unsafe {
            (*frame) = mem::zeroed();
            (*frame).r0 = initial_function as u32;
            (*frame).r1 = arg0 as u32;
            (*frame).pc = trampoline as u32;
            (*frame).psr = 1 << 24; // T bit
            (*frame).return_address = 0xfffffff9; // return to state using MSP and no FP
        }

        self.frame = frame
    }
}

fn trampoline(initial_function: fn(usize), arg0: usize) {
    // info!(
    //     "cortex-m trampoline: initial function {:#x} arg {:#x}",
    //     initial_function as usize, arg0
    // );

    pw_assert::assert!(Arch::interrupts_enabled());

    // Call the actual initial function of the thread.
    initial_function(arg0);

    // Get a pointer to the current thread and call exit.
    // Note: must let the scope of the lock guard close,
    // since exit_thread() does not return.
    scheduler::exit_thread();

    // Does not reach.
}

// Called by the pendsv handler, returns the new stack to switch to after
// performing some housekeeping.
#[no_mangle]
unsafe fn pendsv_swap_sp(frame: *mut FullExceptionFrame) -> *mut FullExceptionFrame {
    // TODO:
    // save incoming frame to active_thread.archstate
    // clear active_thread
    // return a pointer to the current_thread's sp

    // Clear any local reservations on atomic addresses, in case we were context switched
    // in the middle of an atomic sequence.
    asm!("clrex");

    pw_assert::assert!(in_interrupt_handler());
    pw_assert::assert!(!Arch::interrupts_enabled());

    // Save the incoming frame to the current active thread's arch state, that will function
    // as the context switch frame for when it is returned to later. Clear active thread
    // afterwards.
    let at = get_active_thread();
    // info!("inside pendsv: currently active thread {:08x}", at as usize);
    // info!("old frame {:08x}: pc {:08x}", frame as usize, (*frame).pc);

    pw_assert::assert!(at != core::ptr::null_mut());

    (*at).frame = frame;
    set_active_thread(core::ptr::null_mut());

    // Return the arch frame for the current thread
    let mut ss = SCHEDULER_STATE.lock();
    let newframe = (*ss.get_current_arch_thread_state()).frame;
    // info!(
    //     "new frame {:08x}: pc {:08x}",
    //     newframe as usize,
    //     (*newframe).pc
    // );

    newframe
}

#[no_mangle]
#[naked]
pub unsafe extern "C" fn PendSV() -> ! {
    unsafe {
        naked_asm!(
            "
            push    {{ r4 - r11, lr }}  // save the additional registers
            mov     r0, sp
            sub     sp, 4               // realign the stack to 8 byte boundary
            cpsid   i

            bl      pendsv_swap_sp

            // TODO: should sp be saved before cpsie?
            cpsie   i
            mov     sp, r0
            pop     {{ r4 - r11, pc }}

        "
        )
    }
}
