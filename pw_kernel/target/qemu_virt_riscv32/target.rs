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
#![no_main]

use arch_riscv::{Arch, ArchThreadState};
use kernel::InitKernelState;
use target_common::{declare_target, TargetInterface};
use {console_backend as _, kernel as _};

#[cfg(test)]
use integration_tests as _;

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "QEMU-VIRT-RISCV";

    fn main() -> ! {
        // riscv does not run ctors, so we do it manually. Note that this is
        // required in order to register tests, which is a prerequisite to
        // calling `run_all_tests` below.
        unsafe { target_common::run_ctors() };

        #[cfg(not(test))]
        {
            static mut DEMO_STATE: demo::DemoState<Arch> = demo::DemoState::new(Arch);
            // SAFETY: `main` is only executed once, so we never generate more
            // than one `&mut` reference to `DEMO_STATE`.
            #[allow(static_mut_refs)]
            demo::main(Arch, unsafe { &mut DEMO_STATE });
        }

        #[cfg(test)]
        {
            use riscv_semihosting::debug::*;
            use unittest_core::TestsResult;

            exit(match unittest_core::run_all_tests!() {
                TestsResult::AllPassed => EXIT_SUCCESS,
                TestsResult::SomeFailed => EXIT_FAILURE,
            });

            // `exit` can return under rare circumstances.
            #[allow(unreachable_code, clippy::empty_loop)]
            loop {}
        }
    }
}

declare_target!(Target);

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    use kernel::KernelContext as _;
    Arch::panic()
}

#[riscv_rt::entry]
fn main() -> ! {
    Target::console_init();

    static mut INIT_STATE: InitKernelState<ArchThreadState> = InitKernelState::new();

    // SAFETY: `main` is only executed once, so we never generate more than one
    // `&mut` reference to `INIT_STATE`.
    #[allow(static_mut_refs)]
    kernel::Kernel::main(Arch, unsafe { &mut INIT_STATE });
}
