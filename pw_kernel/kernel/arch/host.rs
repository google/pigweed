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

use pw_log::info;
use pw_status::Result;

use crate::arch::MemoryRegionType;
use crate::scheduler::thread::{Stack, ThreadState};
use crate::scheduler::{SchedulerContext, SchedulerState};
use crate::sync::spinlock::{SpinLock, SpinLockGuard};

mod spinlock;

#[derive(Copy, Clone)]
pub struct Arch;

pub struct ArchThreadState;

impl SchedulerContext for Arch {
    type ThreadState = ArchThreadState;
    type BareSpinLock = spinlock::BareSpinLock;

    unsafe fn context_switch(
        self,
        _sched_state: SpinLockGuard<'_, spinlock::BareSpinLock, SchedulerState<ArchThreadState>>,
        _old_thread_state: *mut ArchThreadState,
        _new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'_, spinlock::BareSpinLock, SchedulerState<ArchThreadState>> {
        pw_assert::panic!("unimplemented");
    }

    fn enable_interrupts() {
        todo!("unimplemented");
    }
    fn disable_interrupts() {
        todo!("");
    }
    fn interrupts_enabled() -> bool {
        todo!("");
    }

    fn get_scheduler_lock(
        self,
    ) -> &'static SpinLock<spinlock::BareSpinLock, SchedulerState<ArchThreadState>> {
        static LOCK: SpinLock<spinlock::BareSpinLock, SchedulerState<ArchThreadState>> =
            SpinLock::new(SchedulerState::new());
        &LOCK
    }
}

impl ThreadState for ArchThreadState {
    const NEW: Self = Self;
    type MemoryConfig = MemoryConfig;

    fn initialize_kernel_frame(
        &mut self,
        _kernel_stack: Stack,
        _memory_config: *const MemoryConfig,
        _initial_function: extern "C" fn(usize, usize),
        _args: (usize, usize),
    ) {
        pw_assert::panic!("unimplemented");
    }

    #[cfg(feature = "user_space")]
    fn initialize_user_frame(
        &mut self,
        _kernel_stack: Stack,
        _memory_config: *const MemoryConfig,
        _initial_sp: usize,
        _entry_point: usize,
        _arg: usize,
    ) -> Result<()> {
        pw_assert::panic!("unimplemented");
    }
}

pub struct Clock;

impl time::Clock for Clock {
    const TICKS_PER_SEC: u64 = 1_000_000;

    fn now() -> time::Instant<Self> {
        time::Instant::from_ticks(0)
    }
}

pub struct MemoryConfig;

impl crate::arch::MemoryConfig for MemoryConfig {
    const KERNEL_THREAD_MEMORY_CONFIG: Self = Self;

    fn range_has_access(
        &self,
        _access_type: MemoryRegionType,
        _start_addr: usize,
        _end_addr: usize,
    ) -> bool {
        false
    }
}

impl crate::KernelContext for Arch {
    type Clock = Clock;

    fn early_init() {
        info!("HOST arch early init");
    }
    fn init() {
        info!("HOST arch init");
    }
}
