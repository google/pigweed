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

use target_common::{declare_target, TargetInterface};

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "MPS2-AN505";

    fn main() -> ! {
        demo::main()
    }
}

declare_target!(Target);

#[cortex_m_rt::entry]
fn main() -> ! {
    // cortex_m_rt does not run ctors, so we do it manually. Note that this is
    // required in order to register tests, which is a prerequisite to calling
    // `run_tests` below.
    unsafe { target_common::run_ctors() };

    #[cfg(not(feature = "test"))]
    kernel::Kernel::main();

    #[cfg(feature = "test")]
    {
        use cortex_m_semihosting::debug::*;
        Target::console_init();
        exit(target_common::run_tests(EXIT_SUCCESS, EXIT_FAILURE));

        // `exit` can return under rare circumstances.
        #[allow(clippy::empty_loop)]
        loop {}
    }
}
