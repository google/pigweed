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

use console_backend as _;
use kernel as _;

use target_common::{declare_target, TargetInterface};

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "Host";

    fn main() -> ! {
        demo::main()
    }

    // Use the default noop implementations.
}

declare_target!(Target);

#[cfg(not(feature = "test"))]
fn main() -> ! {
    kernel::Kernel::main();
}

#[cfg(feature = "test")]
#[no_mangle]
pub extern "C" fn main() -> core::ffi::c_int {
    // Return a C error code here as we're implementing a "bare main".
    use unittest_core::TestsResult;
    match unittest_core::run_all_tests() {
        TestsResult::AllPassed => 0,
        TestsResult::SomeFailed => 1,
    }
}
