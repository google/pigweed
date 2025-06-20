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
use core::mem;

use log_if::debug_if;
use pw_status::Result;

use crate::arch::riscv::protection::MemoryConfig;
use crate::arch::riscv::regs::{MStatusVal, PrivilegeLevel};
use crate::arch::riscv::spinlock::BareSpinLock;
use crate::arch::riscv::Arch;
use crate::scheduler::thread::Stack;
use crate::scheduler::{self, SchedulerContext, SchedulerState};
use crate::sync::spinlock::SpinLockGuard;

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
    #[cfg(feature = "user_space")]
    memory_config: *const MemoryConfig,
}

impl ArchThreadState {
    #[inline(never)]
    fn initialize_frame(
        &mut self,
        kernel_stack: Stack,
        trampoline: extern "C" fn(),
        initial_mstatus: MStatusVal,
        initial_sp: usize,
        (s0, s1, s2): (usize, usize, usize),
    ) {
        let frame: *mut ContextSwitchFrame =
            Stack::aligned_stack_allocation_mut(unsafe { kernel_stack.end_mut() }, 8);

        unsafe {
            // Clear the stack and set up the exception frame such that it would
            // return to the function passed in with arg0 and arg1 passed in the
            // first two argument slots.
            (*frame) = mem::zeroed();
            (*frame).ra = trampoline as usize;
            (*frame).s0 = s0;
            (*frame).s1 = s1;
            (*frame).s2 = s2;
            (*frame).s5 = initial_sp;
            (*frame).s6 = initial_mstatus.0;
        }

        self.frame = frame
    }
}

impl SchedulerContext for super::Arch {
    type ThreadState = ArchThreadState;
    type BareSpinLock = BareSpinLock;
    type Clock = super::timer::Clock;

    #[inline(never)]
    unsafe fn context_switch<'a>(
        self,
        sched_state: SpinLockGuard<'a, BareSpinLock, SchedulerState<ArchThreadState>>,
        old_thread_state: *mut ArchThreadState,
        new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'a, BareSpinLock, SchedulerState<ArchThreadState>> {
        debug_if!(
            LOG_CONTEXT_SWITCH,
            "context switch from frame {:#08x} to frame {:#08x}",
            (*old_thread_state).frame as usize,
            (*new_thread_state).frame as usize,
        );

        // Memory config is swapped before the context switch instead of after.
        // This avoids needing a special case in the user mode thread init to
        // initialize the config.
        //
        // Memory context switch overhead is avoided for threads in the same
        // memory config space.
        #[cfg(feature = "user_space")]
        if (*new_thread_state).memory_config != (*old_thread_state).memory_config {
            (*(*new_thread_state).memory_config).write();
        }

        // Note: there is a small window of time where the new memory configuration
        // is active (above) and the new thread is active (below).  Since this code
        // always executes in M-Mode, it bypasses the memory config and by the
        // time control is returned to user space, the memory config is correct.
        riscv_context_switch(&mut (*old_thread_state).frame, (*new_thread_state).frame);

        sched_state
    }

    fn now(self) -> time::Instant<super::timer::Clock> {
        use time::Clock as _;
        super::timer::Clock::now()
    }

    fn idle() {
        riscv::asm::wfi();
    }

    fn enable_interrupts() {
        unsafe {
            riscv::register::mstatus::set_mie();
        }
    }

    fn disable_interrupts() {
        unsafe {
            riscv::register::mstatus::clear_mie();
        }
    }

    fn interrupts_enabled() -> bool {
        riscv::register::mstatus::read().mie()
    }
}

impl crate::scheduler::thread::ThreadState for ArchThreadState {
    type MemoryConfig = crate::arch::riscv::protection::MemoryConfig;
    const NEW: Self = Self {
        frame: core::ptr::null_mut(),
        #[cfg(feature = "user_space")]
        memory_config: core::ptr::null(),
    };

    #[inline(never)]
    fn initialize_kernel_frame(
        &mut self,
        kernel_stack: Stack,
        memory_config: *const MemoryConfig,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    ) {
        self.memory_config = memory_config;
        self.initialize_frame(
            kernel_stack,
            asm_trampoline,
            MStatusVal::default(),
            0x0,
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
        let mstatus = MStatusVal::default()
            .with_mpie(true)
            .with_spie(true)
            .with_mpp(PrivilegeLevel::User);
        self.initialize_frame(
            kernel_stack,
            asm_user_trampoline,
            mstatus,
            initial_sp as usize,
            (entry_point, arg, 0),
        );

        Ok(())
    }
}

#[no_mangle]
#[unsafe(naked)]
extern "C" fn riscv_context_switch(
    old_frame: *mut *mut ContextSwitchFrame,
    new_frame: *mut ContextSwitchFrame,
) {
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

// Since the context switch frame does not contain the function arg registers,
// pass the initial function and arguments via two of the saved s registers.
#[no_mangle]
#[unsafe(naked)]
extern "C" fn asm_user_trampoline() {
    naked_asm!(
        "
                // Store the kernel stack pointer in mscratch.
                csrw    mscratch, sp

                // Set initial SP as passed in by `initialize_frame()`.
                mv      sp, s5

                // Set args for function call.
                mv      a0, s1
                mv      a1, s2

                // Mstatus and Mepc are set up for a return to U-Mode.
                csrw    mstatus, s6
                csrw    mepc, s0
                mret
            "
    )
}

// Since the context switch frame does not contain the function arg registers,
// pass the initial function and arguments via two of the saved s registers.
#[no_mangle]
#[unsafe(naked)]
extern "C" fn asm_trampoline() {
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
    scheduler::exit_thread(Arch);

    // Does not reach.
}
