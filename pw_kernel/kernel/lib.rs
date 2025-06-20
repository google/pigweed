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

use pw_log::info;

pub mod arch;
#[cfg(not(feature = "std_panic_handler"))]
mod panic;
pub mod scheduler;
pub mod sync;
mod syscall;
mod target;

// Used by the `init_thread!` macro.
pub use arch::{Arch, MemoryRegion, MemoryRegionType};
use kernel_config::{KernelConfig, KernelConfigInterface};
pub use scheduler::thread::{Process, Stack, Thread};
#[doc(hidden)]
pub use scheduler::thread::{StackStorage, StackStorageExt};
pub use scheduler::timer::Duration;
use scheduler::timer::TimerQueue;
pub use scheduler::{sleep_until, start_thread, yield_timeslice};
use scheduler::{SchedulerContext, SchedulerState, SchedulerStateContext as _};
use sync::spinlock::SpinLock;

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    Arch::panic();
}

pub trait KernelContext: Sized {
    fn early_init(self) {}
    fn init(self) {}

    fn panic() -> ! {
        #[allow(clippy::empty_loop)]
        loop {}
    }
}

pub trait KernelStateContext: SchedulerContext + KernelContext {
    fn get_state(self) -> &'static KernelState<Self>;
}

pub struct KernelState<C: KernelStateContext> {
    scheduler: SpinLock<C::BareSpinLock, SchedulerState<C::ThreadState>>,
    timer_queue: SpinLock<C::BareSpinLock, TimerQueue<C::Clock>>,
}

impl<C: KernelStateContext> KernelState<C> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            scheduler: SpinLock::new(SchedulerState::new()),
            timer_queue: SpinLock::new(TimerQueue::new()),
        }
    }
}

pub struct Kernel {}

// Module re-exporting modules into a scope that can be referenced by macros
// in this crate.
#[doc(hidden)]
pub mod macro_exports {
    pub use pw_assert;
}

impl Kernel {
    pub fn main() -> ! {
        target::console_init();
        info!("Welcome to Maize on {}!", target::name() as &str);

        Arch.early_init();

        // Prepare the scheduler for thread initialization.
        scheduler::initialize(Arch);

        // SAFETY: The `main` function thread is never executed more than once.
        let bootstrap_thread = unsafe {
            init_thread!(
                "bootstrap",
                bootstrap_thread_entry,
                KernelConfig::KERNEL_STACK_SIZE_BYTES
            )
        };
        info!("created thread, bootstrapping");

        // special case where we bootstrap the system by half context switching to this thread
        scheduler::bootstrap_scheduler(Arch, bootstrap_thread);

        // never get to here
    }
}

// completion of main in thread context
fn bootstrap_thread_entry(_arg0: usize, _arg1: usize) {
    info!("Welcome to the first thread, continuing bootstrap");
    pw_assert::assert!(Arch::interrupts_enabled());

    Arch.init();

    Arch.get_scheduler().lock().dump_all_threads();

    // SAFETY: The bootstrap thread is never executed more than once.
    let idle_thread = unsafe {
        init_thread!(
            "idle",
            idle_thread_entry,
            KernelConfig::KERNEL_STACK_SIZE_BYTES
        )
    };

    Arch::get_scheduler(Arch).lock().dump_all_threads();

    scheduler::start_thread(Arch, idle_thread);

    target::main()
}

fn idle_thread_entry(_arg0: usize, _arg1: usize) {
    // Fake idle thread to keep the runqueue from being empty if all threads are blocked.
    pw_assert::assert!(Arch::interrupts_enabled());
    loop {
        Arch::idle();
    }
}

#[doc(hidden)]
pub mod __private {
    /// Takes a mutable reference to a global static.
    ///
    /// # Safety
    ///
    /// Each invocation of `static_mut_ref!` must be executed at most once at
    /// run time.
    #[doc(hidden)] // `#[macro_export]` bypasses this module's `#[doc(hidden)]`
    #[macro_export]
    macro_rules! static_mut_ref {
        ($ty:ty = $value:expr) => {{
            static mut __STATIC: $ty = $value;
            // SAFETY: The caller promises that this macro will be executed at
            // most once, and so taking a `&mut` reference to this global
            // static, which is defined per-call site, will not violate
            // aliasing.
            #[allow(static_mut_refs)]
            &mut __STATIC
        }};
    }

    pub type ArchThreadState =
        <crate::arch::Arch as crate::scheduler::SchedulerContext>::ThreadState;

    pub use foreign_box;

    pub use crate::arch::Arch;
}
