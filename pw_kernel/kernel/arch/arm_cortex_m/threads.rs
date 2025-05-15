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

use core::arch::asm;
use core::mem::{self, MaybeUninit};

use cortex_m::peripheral::SCB;

// use pw_log::info;

use crate::arch::arm_cortex_m::exceptions::{
    exception, ExcReturn, ExcReturnFrameType, ExcReturnMode, ExcReturnRegisterStacking,
    ExcReturnStack, ExceptionFrame, KernelExceptionFrame, RetPsrVal,
};
use crate::arch::arm_cortex_m::regs::msr::{ControlVal, Spsel};
use crate::arch::arm_cortex_m::{in_interrupt_handler, Arch};
use crate::arch::ArchInterface;
use crate::scheduler::{self, thread::Stack, SchedulerState, SCHEDULER_STATE};
use crate::sync::spinlock::SpinLockGuard;

const STACK_ALIGNMENT: usize = 8;

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
    frame: *mut KernelExceptionFrame,
}

impl ArchThreadState {
    #[inline(never)]
    fn initialize_frame(
        &mut self,
        user_frame: *mut ExceptionFrame,
        kernel_frame: *mut KernelExceptionFrame,
        control: ControlVal,
        psp: u32,
        return_address: ExcReturn,
        initial_pc: usize,
        (r0, r1, r2): (usize, usize, usize),
    ) {
        // Clear the stack and set up the exception frame such that it would
        // return to the function passed in with arg0 and arg1 passed in the
        // first two argument slots.
        unsafe {
            (*user_frame) = mem::zeroed();
            (*user_frame).r0 = r0 as u32;
            (*user_frame).r1 = r1 as u32;
            (*user_frame).r2 = r2 as u32;
            (*user_frame).pc = initial_pc as u32;
            (*user_frame).psr = RetPsrVal(0).with_t(true);
            (*kernel_frame) = mem::zeroed();
            (*kernel_frame).psp = psp;
            (*kernel_frame).control = control;
            (*kernel_frame).return_address = return_address.bits() as u32;
        }
        self.frame = kernel_frame;
    }
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
        //     new_thread.id(),
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
            pw_assert::debug_assert!(Arch::interrupts_enabled());

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

    fn initialize_kernel_frame(
        &mut self,
        kernel_stack: Stack,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    ) {
        let user_frame = Stack::aligned_stack_allocation(
            kernel_stack.end(),
            size_of::<ExceptionFrame>(),
            STACK_ALIGNMENT,
        );

        let kernel_frame = Stack::aligned_stack_allocation(
            user_frame,
            size_of::<KernelExceptionFrame>(),
            // For kernel threads, kernel_frame needs to come immediately after
            // the user stack regardless of alignment because that is what the
            // exception wrapper assembly expects.
            size_of::<usize>(),
        );

        self.initialize_frame(
            user_frame as *mut ExceptionFrame,
            kernel_frame as *mut KernelExceptionFrame,
            ControlVal::default()
                .with_npriv(false)
                .with_spsel(Spsel::Main),
            0x0,
            ExcReturn::new(
                ExcReturnStack::MainSecure,
                ExcReturnRegisterStacking::Default,
                ExcReturnFrameType::Standard,
                ExcReturnMode::ThreadSecure,
            ),
            trampoline as usize,
            (initial_function as usize, args.0, args.1),
        );
    }

    #[cfg(feature = "user_space")]
    fn initialize_user_frame(
        &mut self,
        kernel_stack: Stack,
        initial_sp: *mut MaybeUninit<u8>,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    ) {
        let user_frame = Stack::aligned_stack_allocation(
            initial_sp,
            size_of::<ExceptionFrame>(),
            STACK_ALIGNMENT,
        );

        let kernel_frame = Stack::aligned_stack_allocation(
            kernel_stack.end(),
            size_of::<KernelExceptionFrame>(),
            STACK_ALIGNMENT,
        );

        self.initialize_frame(
            user_frame as *mut ExceptionFrame,
            kernel_frame as *mut KernelExceptionFrame,
            ControlVal::default()
                .with_npriv(true)
                .with_spsel(Spsel::Process),
            user_frame as u32,
            ExcReturn::new(
                ExcReturnStack::ThreadSecure,
                ExcReturnRegisterStacking::Default,
                ExcReturnFrameType::Standard,
                ExcReturnMode::ThreadSecure,
            ),
            initial_function as usize,
            (args.0, args.1, 0x0),
        );
    }
}

extern "C" fn trampoline(initial_function: extern "C" fn(usize, usize), arg0: usize, arg1: usize) {
    // info!(
    //     "cortex-m trampoline: initial function {:#x} arg {:#x}",
    //     initial_function as usize, arg0
    // );

    pw_assert::assert!(Arch::interrupts_enabled());

    // Call the actual initial function of the thread.
    initial_function(arg0, arg1);

    // Get a pointer to the current thread and call exit.
    // Note: must let the scope of the lock guard close,
    // since exit_thread() does not return.
    scheduler::exit_thread();

    // Does not reach.
}

// Called by the pendsv handler, returns the new stack to switch to after
// performing some housekeeping.
#[exception(exception = "PendSV", disable_interrupts)]
#[no_mangle]
extern "C" fn pendsv_swap_sp(frame: *mut KernelExceptionFrame) -> *mut KernelExceptionFrame {
    // TODO:
    // save incoming frame to active_thread.archstate
    // clear active_thread
    // return a pointer to the current_thread's sp

    // Clear any local reservations on atomic addresses, in case we were context switched
    // in the middle of an atomic sequence.
    unsafe { asm!("clrex") };

    pw_assert::assert!(in_interrupt_handler());
    pw_assert::assert!(!Arch::interrupts_enabled());

    // Save the incoming frame to the current active thread's arch state, that will function
    // as the context switch frame for when it is returned to later. Clear active thread
    // afterwards.
    let active_thread = unsafe { get_active_thread() };
    // info!(
    //     "inside pendsv: currently active thread {:08x}",
    //     active_thread as usize
    // );
    // info!("old frame {:08x}: pc {:08x}", frame as usize, (*frame).pc);

    pw_assert::assert!(active_thread != core::ptr::null_mut());

    unsafe {
        (*active_thread).frame = frame;

        set_active_thread(core::ptr::null_mut());
    }

    // Return the arch frame for the current thread
    let mut sched_state = SCHEDULER_STATE.lock();
    let newframe = unsafe { (*sched_state.get_current_arch_thread_state()).frame };
    // info!(
    //     "new frame {:08x}: pc {:08x}",
    //     newframe as usize,
    //     (*newframe).pc
    // );

    newframe
}
