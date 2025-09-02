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

use pw_atomic::AtomicUsize;
use pw_log::info;
pub use time::{Duration, Instant};

pub mod memory;
pub mod object;
#[cfg(not(feature = "std_panic_handler"))]
mod panic;
pub mod scheduler;
pub mod sync;
pub mod syscall;
mod target;

use kernel_config::{KernelConfig, KernelConfigInterface};
pub use memory::{MemoryRegion, MemoryRegionType};
pub use object::NullObjectTable;
#[doc(hidden)]
pub use scheduler::thread::{Process, Stack, StackStorage, StackStorageExt, Thread, ThreadState};
use scheduler::timer::TimerQueue;
use scheduler::{SchedulerState, thread};
pub use scheduler::{sleep_until, start_thread, yield_timeslice};
use sync::spinlock::{BareSpinLock, SpinLock, SpinLockGuard};

use crate::scheduler::{PreemptDisableGuard, ThreadLocalState};
pub use crate::syscall::SyscallArgs;

pub trait Arch: 'static + Copy + thread::ThreadArg {
    type ThreadState: ThreadState;
    type BareSpinLock: BareSpinLock;
    type Clock: time::Clock;
    type AtomicUsize: AtomicUsize;
    type SyscallArgs<'a>: SyscallArgs<'a>;

    /// Switches to a new thread.
    ///
    /// - `sched_state`: A guard for the global `SchedulerState`
    ///   - This may be dropped and re-acquired across this function; the
    ///     returned guard is either still held or newly re-acquired
    /// - `old_thread_state`: The thread we're moving away from
    /// - `new_thread_state`: The thread we're moving to; must match
    ///   `current_thread` and the container for this `ThreadState`
    #[allow(clippy::missing_safety_doc)]
    unsafe fn context_switch(
        self,
        sched_state: SpinLockGuard<'_, Self, SchedulerState<Self>>,
        old_thread_state: *mut Self::ThreadState,
        new_thread_state: *mut Self::ThreadState,
    ) -> SpinLockGuard<'_, Self, SchedulerState<Self>>
    where
        Self: Kernel;

    fn thread_local_state(self) -> &'static ThreadLocalState<Self>;

    fn now(self) -> Instant<Self::Clock>;

    // fill in more arch implementation functions from the kernel here:
    // arch-specific backtracing
    #[allow(dead_code)]
    fn enable_interrupts(self);
    #[allow(dead_code)]
    fn disable_interrupts(self);
    #[allow(dead_code)]
    fn interrupts_enabled(self) -> bool;

    #[allow(dead_code)]
    fn idle(self) {}

    fn early_init(self) {}
    fn init(self) {}

    fn panic() -> ! {
        #[allow(clippy::empty_loop)]
        loop {}
    }
}

pub trait Kernel: Arch + Sync {
    fn get_state(self) -> &'static KernelState<Self>;

    fn get_scheduler(self) -> &'static SpinLock<Self, SchedulerState<Self>> {
        &self.get_state().scheduler
    }

    fn get_timer_queue(self) -> &'static SpinLock<Self, TimerQueue<Self>> {
        &self.get_state().timer_queue
    }
}

pub struct KernelState<K: Kernel> {
    scheduler: SpinLock<K, SchedulerState<K>>,
    timer_queue: SpinLock<K, TimerQueue<K>>,
}

impl<K: Kernel> KernelState<K> {
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
/// struct Arch;
///
/// #[cortex_m_rt::entry]
/// fn main() -> ! {
///     Target::console_init();
///
///     kernel::static_init_state!(static mut INIT_STATE: InitKernelState<Arch>);
///
///     // SAFETY: `main` is only executed once, so we never generate more than one
///     // `&mut` reference to `INIT_STATE`.
///     kernel::Kernel::main(Arch, unsafe { &mut INIT_STATE });
/// }
/// ```
#[macro_export]
macro_rules! static_init_state {
    ($vis:vis static mut $name:ident: InitKernelState<$kernel:ty>) => {
        $vis static mut $name: $crate::InitKernelState<$kernel> = {
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
pub struct ThreadStorage<K: Kernel> {
    pub thread: Thread<K>,
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

pub struct InitKernelState<K: Kernel> {
    #[doc(hidden)]
    pub bootstrap_thread: ThreadStorage<K>,
    #[doc(hidden)]
    pub idle_thread: ThreadStorage<K>,
}

// Module re-exporting modules into a scope that can be referenced by macros
// in this crate.
#[doc(hidden)]
pub mod macro_exports {
    pub use {foreign_box, pw_assert};
}

pub fn main<K: Kernel>(kernel: K, init_state: &'static mut InitKernelState<K>) -> ! {
    let preempt_guard = PreemptDisableGuard::new(kernel);

    target::console_init();
    info!("Welcome to Maize on {}!", target::name() as &str);

    kernel.early_init();

    // Prepare the scheduler for thread initialization.
    scheduler::initialize(kernel);

    let bootstrap_thread = thread::init_thread_in(
        kernel,
        &mut init_state.bootstrap_thread.thread,
        init_state.bootstrap_thread.stack,
        "bootstrap",
        bootstrap_thread_entry,
        &mut init_state.idle_thread,
    );
    info!("created thread, bootstrapping");

    // special case where we bootstrap the system by half context switching to this thread
    scheduler::bootstrap_scheduler(kernel, preempt_guard, bootstrap_thread);

    // never get to here
}

// completion of main in thread context
fn bootstrap_thread_entry<K: Kernel>(
    kernel: K,
    idle_thread_storage: &'static mut ThreadStorage<K>,
) {
    info!("Welcome to the first thread, continuing bootstrap");
    pw_assert::assert!(kernel.interrupts_enabled());

    kernel.init();

    kernel.get_scheduler().lock(kernel).dump_all_threads();

    let idle_thread = thread::init_thread_in(
        kernel,
        &mut idle_thread_storage.thread,
        idle_thread_storage.stack,
        "idle",
        idle_thread_entry,
        0,
    );

    kernel.get_scheduler().lock(kernel).dump_all_threads();

    scheduler::start_thread(kernel, idle_thread);

    target::main()
}

fn idle_thread_entry<K: Kernel>(kernel: K, _arg: usize) {
    // Fake idle thread to keep the runqueue from being empty if all threads are blocked.
    pw_assert::assert!(kernel.interrupts_enabled());
    loop {
        kernel.idle();
    }
}

/// Stores `t` in stack memory and provides a `&'static mut T` to a function
/// which will never return.
///
/// Since `F: FnOnce(...) -> !`, the stack memory holding `t` will never be
/// reclaimed, and so it can live forever (ie, be referenced using `&'static mut
/// T`).
///
/// If `f` unwinds, `with_static` will `loop {}` forever to prevent `t` from
/// being reclaimed.
pub fn with_static<T: 'static, F: FnOnce(&'static mut T)>(t: T, f: F) -> ! {
    struct LoopOnDrop<T>(T);
    impl<T> Drop for LoopOnDrop<T> {
        fn drop(&mut self) {
            #[allow(clippy::empty_loop)]
            loop {}
        }
    }

    let mut t = LoopOnDrop(t);
    let tp: *mut T = &mut t.0;

    // SAFETY: Since we `drop(t)` after the `loop {}`, `t` will only go out of
    // scope if `f` unwinds. If this happens, `LoopOnDrop::drop` will `loop {}`
    // forever before its inner `T` is destructed. Thus, `t.0` will never be
    // destructed, and so it is sound to synthesize a `'static` reference to
    // `t`.
    //
    // See for more analysis: https://github.com/rust-lang/unsafe-code-guidelines/issues/565
    f(unsafe { &mut *tp });

    #[allow(clippy::empty_loop)]
    loop {}
    #[allow(unreachable_code)]
    drop(t);
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
            use $crate::__private::foreign_box::StaticStorage;
            static mut __STATIC: StaticStorage<$ty> = StaticStorage::new();

            // Alias value to allow nested `unsafe` statements without warning.
            let value = $value;

            // SAFETY: The caller promises that this macro will be executed at
            // most once fulfilling `StaticStorage`'s precondition.
            unsafe { __STATIC.init(value) }
        }};
    }

    pub use {foreign_box, kernel_config, time};
}
