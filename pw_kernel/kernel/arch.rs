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

#[cfg(feature = "arch_arm_cortex_m")]
mod arm_cortex_m;
#[cfg(feature = "arch_arm_cortex_m")]
pub use arm_cortex_m::Arch;

#[cfg(feature = "arch_riscv")]
mod riscv;
#[cfg(feature = "arch_riscv")]
pub use riscv::Arch;

#[cfg(feature = "arch_host")]
mod host;
#[cfg(feature = "arch_host")]
pub use host::Arch;

use core::mem::MaybeUninit;

use crate::scheduler::{thread::Stack, SchedulerState};
use crate::sync::spinlock::SpinLockGuard;

pub type ArchThreadState = <Arch as ArchInterface>::ThreadState;

pub trait ThreadState {
    #[allow(dead_code)]
    fn new() -> Self;

    // Switch to this thread.
    // sched_state: a locked spinlockguard for the main SCHEDULER_STATE struct.
    //   Will potentially be dropped and reacquired across this function, and
    //   a copy will be returned (either still held, or newly reacquired).
    // old_thread_state: thread we're moving from.
    // new_thread_state: must match current_thread and the container for this ThreadState.
    unsafe fn context_switch(
        sched_state: SpinLockGuard<'_, SchedulerState>,
        old_thread_state: *mut ArchThreadState,
        new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'_, SchedulerState>;

    /// Initialize the default frame of a kernel thread
    ///
    /// Arranges for the thread to start at `initial_function` with arguments
    /// passed in the first two argument slots.  The stack pointer of the thread
    /// is set to the top of the kernel stack.
    fn initialize_kernel_frame(
        &mut self,
        kernel_stack: Stack,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    );

    /// Initialize the default frame of a user thread
    ///
    /// Arranges for the thread to start at `initial_function` with arguments
    /// passed in the first two argument slots
    #[cfg(feature = "user_space")]
    fn initialize_user_frame(
        &mut self,
        kernel_stack: Stack,
        initial_sp: *mut MaybeUninit<u8>,
        initial_function: extern "C" fn(usize, usize),
        args: (usize, usize),
    );
}

#[derive(Clone, Copy)]
#[allow(dead_code)]
pub enum MemoryRegionType {
    /// Read Only, Non-Executable data.
    ReadOnlyData,

    /// Mode Read/Write, Non-Executable data.
    ReadWriteData,

    /// Mode Read Only, Executable data.
    ReadOnlyExecutable,

    /// Mode Read/Write, Executable data.
    ReadWriteExecutable,

    /// Device MMIO memory.
    Device,
}

/// Architecture independent memory region description.
///
/// `MemoryRegion` provides an architecture independent way to describe a memory
/// regions and its configuration.
#[allow(dead_code)]
pub struct MemoryRegion {
    /// Type of the memory region
    ty: MemoryRegionType,

    /// Start address of the memory region (inclusive)
    start: usize,

    /// Start address of the memory region (exclusive)
    end: usize,
}

pub trait BareSpinLock {
    type Guard<'a>
    where
        Self: 'a;

    // Rust does not support const function in traits.  However it should be
    // considered that the BareSpinlockApi includes:
    //
    // `const fn new() -> Self`

    fn try_lock(&self) -> Option<Self::Guard<'_>>;

    #[inline(always)]
    fn lock(&self) -> Self::Guard<'_> {
        loop {
            if let Some(sentinel) = self.try_lock() {
                return sentinel;
            }
        }
    }

    // TODO - konkers: Add optimized path for functions that know they are in
    // atomic context (i.e. interrupt handlers).
}

pub trait ArchInterface {
    type ThreadState: ThreadState;
    type BareSpinLock: BareSpinLock;
    type Clock: time::Clock;

    fn early_init() {}
    fn init() {}

    fn panic() -> ! {
        #[allow(clippy::empty_loop)]
        loop {}
    }
    // fill in more arch implementation functions from the kernel here:
    // arch-specific backtracing
    #[allow(dead_code)]
    fn enable_interrupts();
    #[allow(dead_code)]
    fn disable_interrupts();
    #[allow(dead_code)]
    fn interrupts_enabled() -> bool;

    #[allow(dead_code)]
    fn idle() {}
}
