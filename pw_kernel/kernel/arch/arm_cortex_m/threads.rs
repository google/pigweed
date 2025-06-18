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
use pw_cast::CastInto as _;
use pw_status::{Error, Result};

use crate::arch::arm_cortex_m::exceptions::{
    exception, ExcReturn, ExcReturnFrameType, ExcReturnMode, ExcReturnRegisterStacking,
    ExcReturnStack, ExceptionFrame, KernelExceptionFrame, RetPsrVal,
};
use crate::arch::arm_cortex_m::protection::MemoryConfig;
use crate::arch::arm_cortex_m::regs::msr::{ControlVal, Spsel};
use crate::arch::arm_cortex_m::spinlock::BareSpinLock;
use crate::arch::arm_cortex_m::{in_interrupt_handler, Arch};
use crate::arch::{MemoryConfig as _, MemoryRegionType};
use crate::scheduler::thread::Stack;
use crate::scheduler::{self, SchedulerContext, SchedulerState, SchedulerStateContext as _};
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
    memory_config: *const MemoryConfig,
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
            (*user_frame).r0 = r0.cast_into();
            (*user_frame).r1 = r1.cast_into();
            (*user_frame).r2 = r2.cast_into();
            (*user_frame).pc = initial_pc.cast_into();
            (*user_frame).psr = RetPsrVal(0).with_t(true);
            (*kernel_frame) = mem::zeroed();
            (*kernel_frame).psp = psp;
            (*kernel_frame).control = control;
            (*kernel_frame).return_address = return_address.bits().cast_into();
        }
        self.frame = kernel_frame;
    }
}

impl SchedulerContext for Arch {
    type ThreadState = ArchThreadState;
    type BareSpinLock = BareSpinLock;

    unsafe fn context_switch<'a>(
        self,
        mut sched_state: SpinLockGuard<'a, BareSpinLock, SchedulerState<ArchThreadState>>,
        old_thread_state: *mut ArchThreadState,
        new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'a, BareSpinLock, SchedulerState<ArchThreadState>> {
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

            sched_state = Arch::get_scheduler_lock(Arch).lock();
        } else {
            // in interrupt context the pendsv should have already triggered it
            pw_assert::assert!(SCB::is_pendsv_pending());
        }
        sched_state
    }

    fn enable_interrupts() {
        unsafe {
            cortex_m::interrupt::enable();
        }
    }

    fn disable_interrupts() {
        cortex_m::interrupt::disable();
    }

    fn interrupts_enabled() -> bool {
        // It's a complicated concept in cortex-m:
        // If PRIMASK is inactive, then interrupts are 100% disabled otherwise
        // if the current interrupt priority level is not zero (BASEPRI register) interrupts
        // at that level are not allowed. For now we're treating nonzero as full disabled.
        let primask = cortex_m::register::primask::read();
        let basepri = cortex_m::register::basepri::read();
        primask.is_active() && (basepri == 0)
    }

    fn idle() {
        cortex_m::asm::wfi();
    }
}

impl crate::scheduler::thread::ThreadState for ArchThreadState {
    type MemoryConfig = crate::arch::arm_cortex_m::protection::MemoryConfig;

    const NEW: Self = Self {
        frame: core::ptr::null_mut(),
        memory_config: core::ptr::null(),
    };

    fn initialize_kernel_frame(
        &mut self,
        kernel_stack: Stack,
        memory_config: *const MemoryConfig,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    ) {
        self.memory_config = memory_config;
        let user_frame: *mut ExceptionFrame =
            Stack::aligned_stack_allocation_mut(unsafe { kernel_stack.end_mut() }, STACK_ALIGNMENT);

        let kernel_frame: *mut KernelExceptionFrame = Stack::aligned_stack_allocation_mut(
            user_frame.cast(),
            // For kernel threads, kernel_frame needs to come immediately after
            // the user stack regardless of alignment because that is what the
            // exception wrapper assembly expects.
            size_of::<usize>(),
        );

        // TODO: This is unsound: `user_frame` and `kernel_frame` need to be
        // `*mut` to preserve the ability to mutate their referents in the Rust
        // memory model.
        self.initialize_frame(
            user_frame,
            kernel_frame.cast::<KernelExceptionFrame>(),
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
        memory_config: *const MemoryConfig,
        initial_sp: usize,
        entry_point: usize,
        arg: usize,
    ) -> Result<()> {
        self.memory_config = memory_config;

        // In order to use an exception return to switch to user space, an
        // exception stack frame needs to be written to the user stack.  This
        // is first aligned then checked to make sure the process, through the
        // associated `MemoryConfig`, has access to the underlying memory.
        let user_frame: *mut ExceptionFrame = Stack::aligned_stack_allocation_mut(
            core::ptr::with_exposed_provenance_mut::<MaybeUninit<u8>>(initial_sp),
            STACK_ALIGNMENT,
        );

        if !unsafe { &*memory_config }.has_access(MemoryRegionType::ReadWriteData, user_frame) {
            return Err(Error::PermissionDenied);
        }

        let kernel_frame: *mut KernelExceptionFrame =
            Stack::aligned_stack_allocation_mut(unsafe { kernel_stack.end_mut() }, STACK_ALIGNMENT);

        self.initialize_frame(
            user_frame,
            kernel_frame,
            ControlVal::default()
                .with_npriv(true)
                .with_spsel(Spsel::Process),
            user_frame.expose_provenance().cast_into(),
            ExcReturn::new(
                ExcReturnStack::ThreadSecure,
                ExcReturnRegisterStacking::Default,
                ExcReturnFrameType::Standard,
                ExcReturnMode::ThreadSecure,
            ),
            entry_point,
            (arg, 0x0, 0x0),
        );

        Ok(())
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
    scheduler::exit_thread(Arch);

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

    pw_assert::assert!(active_thread != core::ptr::null_mut());

    unsafe {
        (*active_thread).frame = frame;

        set_active_thread(core::ptr::null_mut());
    }

    // Return the arch frame for the current thread
    let mut sched_state = Arch.get_scheduler_lock().lock();
    let new_thread = unsafe { sched_state.get_current_arch_thread_state() };
    // info!(
    //     "new frame {:08x}: pc {:08x}",
    //     newframe as usize,
    //     (*newframe).pc
    // );
    //   pw_log::info!("context switch {:08x}", new_thread as usize);

    // Memory context switch overhead is avoided for threads in the same
    // memory config space.
    #[cfg(feature = "user_space")]
    unsafe {
        if (*new_thread).memory_config != (*active_thread).memory_config {
            (*(*new_thread).memory_config).write();
        }
    }

    unsafe { (*new_thread).frame }
}
