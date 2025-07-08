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
#![no_std]

#[cfg(test)]
mod tests {
    // TODO: davidroth - This won't work with out of tree architecture.
    // We need to re-visit integration testing to resolve this issue and
    // also not require each target to opt into integration tests.

    #[cfg(feature = "arch_arm_cortex_m")]
    use arch_arm_cortex_m::Arch;
    #[cfg(feature = "arch_riscv")]
    use arch_riscv::Arch;
    use kernel::sync::spinlock::{BareSpinLock, SpinLock};
    use unittest::test;

    type ConcreteBareSpinLock = <Arch as kernel::scheduler::SchedulerContext>::BareSpinLock;

    #[test]
    fn bare_try_lock_returns_correct_value() -> unittest::Result<()> {
        let lock = ConcreteBareSpinLock::new();

        {
            let _sentinel = lock.lock();
            unittest::assert_true!(lock.try_lock().is_none());
        }

        unittest::assert_true!(lock.try_lock().is_some());

        Ok(())
    }

    #[test]
    fn try_lock_returns_correct_value() -> unittest::Result<()> {
        let lock = SpinLock::<ConcreteBareSpinLock, _>::new(false);

        {
            let mut guard = lock.lock();
            *guard = true;
            unittest::assert_true!(lock.try_lock().is_none());
        }

        let guard = lock.lock();
        unittest::assert_true!(*guard);

        Ok(())
    }
}
