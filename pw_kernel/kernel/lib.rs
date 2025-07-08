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

pub mod memory;
#[cfg(not(feature = "std_panic_handler"))]
mod panic;
pub mod scheduler;
pub mod sync;
pub mod syscall;
mod target;

use kernel_config::{KernelConfig, KernelConfigInterface};
pub use memory::{MemoryRegion, MemoryRegionType};
use scheduler::thread::{self, ThreadState};
pub use scheduler::thread::{Process, Stack, Thread};
#[doc(hidden)]
pub use scheduler::thread::{StackStorage, StackStorageExt};
pub use scheduler::timer::Duration;
use scheduler::timer::TimerQueue;
pub use scheduler::{sleep_until, start_thread, yield_timeslice};
use scheduler::{SchedulerContext, SchedulerState, SchedulerStateContext};
use sync::spinlock::SpinLock;

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

/// Initializes an [`InitKernelState`] in static storage.
///
/// # Examples
///
/// Here's an example of using `static_init_state!` on Cortex-M:
///
/// ```
/// struct ArchThreadState;
///
/// #[cortex_m_rt::entry]
/// fn main() -> ! {
///     Target::console_init();
///
///     kernel::static_init_state!(static mut INIT_STATE: InitKernelState<ArchThreadState>);
///
///     // SAFETY: `main` is only executed once, so we never generate more than one
///     // `&mut` reference to `INIT_STATE`.
///     kernel::Kernel::main(Arch, unsafe { &mut INIT_STATE });
/// }
/// ```
#[macro_export]
macro_rules! static_init_state {
    ($vis:vis static mut $name:ident: InitKernelState<$arch:ty>) => {
        $vis static mut $name: $crate::InitKernelState<$arch> = {
            use $crate::__private::kernel_config;
            use kernel_config::KernelConfigInterface as _;
            use kernel::StackStorageExt as _;

            type Stack = $crate::StackStorage<{ kernel_config::KernelConfig::KERNEL_STACK_SIZE_BYTES }>;
            static mut BOOTSTRAP_STACK: Stack = Stack::ZEROED;
            static mut IDLE_STACK: Stack = Stack::ZEROED;

            $crate::InitKernelState {
                bootstrap_thread: $crate::ThreadStorage {
                    thread: $crate::Thread::new("bootstrap"),
                    // SAFETY: We're in a block used to initialize a `static`,
                    // which is only executed once.
                    stack: unsafe { &mut BOOTSTRAP_STACK },
                },
                idle_thread: $crate::ThreadStorage {
                    thread: $crate::Thread::new("idle"),
                    // SAFETY: We're in a block used to initialize a `static`,
                    // which is only executed once.
                    stack: unsafe { &mut IDLE_STACK },
                },
            }
        };
    };
}

#[doc(hidden)]
pub struct ThreadStorage<S: ThreadState> {
    pub thread: Thread<S>,
    // We store the stack out of line so that it is treated as a separate
    // allocation from the containing `ThreadStorage`. The stack itself is
    // zero-initialized, while the `Thread` is not. If we include the stack in
    // the same allocation, then the entire allocation must be placed in the
    // binary itself (in `.data`). By contrast, if it is out of line, then the
    // fact that the whole stack is zero-initialized means that the linker can
    // put it in `.bss` and omit its contents from the binary entirely.
    //
    // See https://pwrev.dev/303372 (Ie93cfc52215753e1b4482d5fe3058dc53fcfa86a)
    // for more information.
    pub stack: &'static mut StackStorage<{ KernelConfig::KERNEL_STACK_SIZE_BYTES }>,
}

pub struct InitKernelState<S: ThreadState> {
    #[doc(hidden)]
    pub bootstrap_thread: ThreadStorage<S>,
    #[doc(hidden)]
    pub idle_thread: ThreadStorage<S>,
}

pub struct Kernel {}

// Module re-exporting modules into a scope that can be referenced by macros
// in this crate.
#[doc(hidden)]
pub mod macro_exports {
    pub use pw_assert;
}

impl Kernel {
    pub fn main<C: KernelStateContext>(
        ctx: C,
        init_state: &'static mut InitKernelState<C::ThreadState>,
    ) -> ! {
        target::console_init();
        info!("Welcome to Maize on {}!", target::name() as &str);

        ctx.early_init();

        // Prepare the scheduler for thread initialization.
        scheduler::initialize(ctx);

        let bootstrap_thread = thread::init_thread_in(
            ctx,
            &mut init_state.bootstrap_thread.thread,
            init_state.bootstrap_thread.stack,
            "bootstrap",
            bootstrap_thread_entry,
            &mut init_state.idle_thread,
        );
        info!("created thread, bootstrapping");

        // special case where we bootstrap the system by half context switching to this thread
        scheduler::bootstrap_scheduler(ctx, bootstrap_thread);

        // never get to here
    }
}

// completion of main in thread context
fn bootstrap_thread_entry<C: KernelStateContext>(
    ctx: C,
    idle_thread_storage: &'static mut ThreadStorage<C::ThreadState>,
) {
    info!("Welcome to the first thread, continuing bootstrap");
    pw_assert::assert!(ctx.interrupts_enabled());

    ctx.init();

    ctx.get_scheduler().lock().dump_all_threads();

    let idle_thread = thread::init_thread_in(
        ctx,
        &mut idle_thread_storage.thread,
        idle_thread_storage.stack,
        "idle",
        idle_thread_entry,
        0,
    );

    ctx.get_scheduler().lock().dump_all_threads();

    scheduler::start_thread(ctx, idle_thread);

    target::main()
}

fn idle_thread_entry<C: KernelStateContext>(ctx: C, _arg: usize) {
    // Fake idle thread to keep the runqueue from being empty if all threads are blocked.
    pw_assert::assert!(ctx.interrupts_enabled());
    loop {
        ctx.idle();
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

    pub use {foreign_box, kernel_config};
}
