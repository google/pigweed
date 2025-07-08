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

use console_backend as _;

use arch_arm_cortex_m::{Arch, ArchThreadState};
use target_common::{declare_target, TargetInterface};
mod userspace_demo_codegen;

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "MPS2-AN505 Userspace Demo";

    fn main() -> ! {
        userspace_demo_codegen::start();
        loop {}
    }
}

declare_target!(Target);

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    use kernel::KernelContext as _;
    Arch::panic()
}

#[cortex_m_rt::entry]
fn main() -> ! {
    kernel::static_init_state!(static mut INIT_STATE: InitKernelState<ArchThreadState>);

    // SAFETY: `main` is only executed once, so we never generate more than one
    // `&mut` reference to `INIT_STATE`.
    #[allow(static_mut_refs)]
    kernel::Kernel::main(Arch, unsafe { &mut INIT_STATE });
}
