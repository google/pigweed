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
#![feature(type_alias_impl_trait)]

use console_backend as _;
use panic_halt as _;

use cortex_m_rt::entry;
use cortex_m_semihosting::debug;
use pw_log::{error, info};
use target::{Target, TargetInterface};

// #[no_mangle]
// pub extern "C" fn _exit(_status: u32) {
//     debug::exit(debug::EXIT_SUCCESS);
// }

type CtorFn = unsafe extern "C" fn() -> usize;
extern "C" {
    static __init_array_start: CtorFn;
    static __init_array_end: CtorFn;
}

fn run_ctors() {
    unsafe {
        let start_ptr: *const CtorFn = &__init_array_start;
        let end_ptr: *const CtorFn = &__init_array_end;
        let num_ctors = end_ptr.offset_from(start_ptr) as usize;
        let ctors = core::slice::from_raw_parts(start_ptr, num_ctors);
        for ctor in ctors {
            let _ = ctor();
        }
    }
}

#[entry]
fn main() -> ! {
    // cortex_m_rt does not run ctors so we need to the that manually.
    run_ctors();

    Target::console_init();
    info!("=============== Unit Test Runner ===============");

    let mut success = true;
    unittest_core::for_each_test(|test| {
        info!("[{}] running", test.desc.name);
        match test.test_fn {
            unittest_core::TestFn::StaticTestFn(f) => {
                if let Err(e) = f() {
                    error!(
                        "[{}] FAILED: {}:{} - {}",
                        test.desc.name, e.file, e.line, e.message
                    );
                    success = false;
                } else {
                    info!("[{}] PASSED", test.desc.name);
                }
            }
        };
    });
    match success {
        true => debug::exit(debug::EXIT_SUCCESS),
        false => debug::exit(debug::EXIT_FAILURE),
    }

    #[allow(clippy::empty_loop)]
    loop {}
}
