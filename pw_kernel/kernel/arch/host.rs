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

use crate::arch::ArchInterface;
use crate::scheduler::{SchedulerState, Stack};
use crate::sync::spinlock::SpinLockGuard;

mod spinlock;

pub struct ThreadState {}

impl super::ThreadState for ThreadState {
    fn new() -> Self {
        Self {}
    }

    unsafe fn context_switch(
        mut _sched_state: SpinLockGuard<'_, SchedulerState>,
        _old_thread_state: *mut ThreadState,
        _new_thread_state: *mut ThreadState,
    ) -> SpinLockGuard<'_, SchedulerState> {
        panic!("unimplemented");
    }
    fn initialize_frame(&mut self, _stack: Stack, _initial_function: fn(usize), _arg0: usize) {
        panic!("unimplemented");
    }
}

pub struct Arch {}

impl ArchInterface for Arch {
    type ThreadState = ThreadState;
    type BareSpinLock = spinlock::BareSpinLock;

    fn early_init() {
        info!("HOST arch early init");
    }
    fn init() {
        info!("HOST arch init");
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
}
