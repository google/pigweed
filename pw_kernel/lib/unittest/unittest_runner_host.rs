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
use pw_log::{error, info};

#[no_mangle]
pub extern "C" fn main() -> core::ffi::c_int {
    let mut success = true;
    unittest_core::for_each_test(|test| {
        info!("[{}] running", test.desc.name as &str);
        match test.test_fn {
            unittest_core::TestFn::StaticTestFn(f) => {
                if let Err(e) = f() {
                    error!(
                        "[{}] FAILED: {}:{} - {}",
                        test.desc.name as &str, e.file as &str, e.line as u32, e.message as &str
                    );
                    success = false;
                } else {
                    info!("[{}] PASSED", test.desc.name as &str);
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
