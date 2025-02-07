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
#![feature(type_alias_impl_trait)]
use console_backend as _;
use pw_log::{error, info};

#[no_mangle]
pub extern "C" fn main() -> core::ffi::c_int {
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
        }
    });

    // Return a C error code here as we're implementing a "bare main".
    match success {
        true => 0,
        false => 1,
    }
}
