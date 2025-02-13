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

#[cfg(feature = "arch_host")]
mod host;
#[cfg(feature = "arch_host")]
pub use host::Arch;

use crate::scheduler::{SchedulerState, Stack, Thread};
use spinlock::SpinLockGuard;

pub trait ThreadState {
    #[allow(dead_code)]
    fn new() -> Self;

    // Switch to this thread.
    // sched_state: a locked spinlockguard for the main SCHEDULER_STATE struct.
    //   Will potentially be dropped and reacquired across this function, and
    //   a copy will be returned (either still held, or newly reacquired).
    // old_thread: thread we're moving from.
    // new_thread: must match current_thread and the container for this ThreadState.

    #[allow(dead_code)]
    fn context_switch<'a>(
        sched_state: SpinLockGuard<'a, SchedulerState>,
        old_thread: &mut Thread,
        new_thread: &mut Thread,
    ) -> SpinLockGuard<'a, SchedulerState>;

    // Initialize the default frame of the thread which arranges for it to start at the initial_function
    // with one argument passed in the first argument slot.
    #[allow(dead_code)]
    fn initialize_frame(&mut self, stack: Stack, initial_function: fn(usize), arg0: usize);
}

pub trait ArchInterface {
    type ThreadState: ThreadState;

    fn early_init() {}
    fn init() {}

    // fill in more arch implementation functions from the kernel here:
    // TODO: interrupt management
    // arch-specific backtracing
}
