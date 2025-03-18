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

use crate::scheduler::{SchedulerState, Stack};
use crate::sync::spinlock::SpinLockGuard;

pub struct ArchThreadState {}

impl super::super::ThreadState for ArchThreadState {
    fn new() -> Self {
        Self {}
    }

    unsafe fn context_switch<'a>(
        sched_state: SpinLockGuard<'a, SchedulerState>,
        _old_thread_state: *mut ArchThreadState,
        _new_thread_state: *mut ArchThreadState,
    ) -> SpinLockGuard<'a, SchedulerState> {
        sched_state
    }

    #[inline(never)]
    fn initialize_frame(&mut self, _stack: Stack, _initial_function: fn(usize), _arg0: usize) {}
}
