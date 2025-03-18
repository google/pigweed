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

use super::ArchInterface;

mod spinlock;
mod threads;
mod timer;

pub struct Arch {}

impl ArchInterface for Arch {
    type ThreadState = threads::ArchThreadState;
    type BareSpinLock = spinlock::BareSpinLock;
    type Clock = timer::Clock;

    fn early_init() {}

    fn init() {}

    fn enable_interrupts() {}

    fn disable_interrupts() {}

    fn interrupts_enabled() -> bool {
        true
    }

    fn idle() {}

    fn panic() -> ! {
        #[allow(clippy::empty_loop)]
        loop {}
    }
}
