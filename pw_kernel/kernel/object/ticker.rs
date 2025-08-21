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

use foreign_box::{ForeignBox, ForeignRc};
use pw_status::Result;
use time::{Duration, Instant};

use crate::Kernel;
use crate::object::{KernelObject, ObjectBase, Signals};
use crate::scheduler::timer::{Timer, TimerCallback};

/// Demo kernel object that signals based off of a timer.
pub struct TickerObject<K: Kernel> {
    base: ObjectBase<K>,
}

impl<K: Kernel> TickerObject<K> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            base: ObjectBase::new(),
        }
    }
    pub fn tick(&self, kernel: K) {
        self.base.signal(kernel, Signals(0x1));
        self.base.signal(kernel, Signals(0));
    }
}

impl<K: Kernel> KernelObject<K> for TickerObject<K> {
    fn object_wait(
        &self,
        kernel: K,
        signal_mask: Signals,
        deadline: Instant<K::Clock>,
    ) -> Result<()> {
        self.base.wait_until(kernel, signal_mask, deadline).map(|_|
                 //  wait result TBD
            ())
    }
}

pub struct TickerCallback<K: Kernel> {
    ticker: ForeignRc<K::AtomicUsize, TickerObject<K>>,
}

impl<K: Kernel> TickerCallback<K> {
    #[must_use]
    pub fn new(ticker: ForeignRc<K::AtomicUsize, TickerObject<K>>) -> Self {
        Self { ticker }
    }
}

impl<K: Kernel> TimerCallback<K> for TickerCallback<K> {
    fn callback(
        &mut self,
        kernel: K,
        mut timer: foreign_box::ForeignBox<Timer<K>>,
        now: Instant<<K>::Clock>,
    ) -> Option<ForeignBox<Timer<K>>> {
        self.ticker.tick(kernel);
        timer.set(now + Duration::<K::Clock>::from_secs(1));
        Some(timer)
    }
}
