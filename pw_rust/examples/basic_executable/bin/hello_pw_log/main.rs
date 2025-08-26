// Copyright 2023 The Pigweed Authors
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

#![no_main]
#![cfg_attr(not(feature = "std"), no_std)]
#![warn(clippy::all)]

#[path = "../../other.rs"]
mod other;

unsafe extern "C" {
    // This external function links to the pw_log C backend specified by the
    // target. It will be replaced by pw_log rust crate once we have third party
    // support for GN.
    pub fn pw_Log(
        level: i32,
        flags: u32,
        module_name: *const i8,
        file_name: *const i8,
        line_number: i32,
        function_name: *const i8,
        message: *const i8,
        ...
    );
}

const PW_LOG_LEVEL_INFO: i32 = 2;

macro_rules! pw_log_info {
  ($($args:expr),*) => (
      unsafe {
        pw_Log(
          PW_LOG_LEVEL_INFO,
          0,
          "\0".as_ptr() as *const i8,
          "\0".as_ptr() as *const i8,
          i32::try_from(line!()).unwrap(),
          "main\0".as_ptr() as *const i8,
          $($args),*);
      }
  );
}

#[unsafe(no_mangle)]
pub extern "C" fn main() -> isize {
    pw_log_info!("Hello, Pigweed!\0".as_ptr() as *const i8);

    // ensure we can run code from other modules in the main crate
    pw_log_info!("%d\0".as_ptr() as *const i8, other::foo());

    // ensure we can run code from dependent libraries
    pw_log_info!(
        "%d\0".as_ptr() as *const i8,
        a::RequiredA::default().required_b.value
    );
    pw_log_info!("%d\0".as_ptr() as *const i8, c::value());

    pw_log_info!(
        "%d\0".as_ptr() as *const i8,
        proc_macro::fn_like_proc_macro!(123)
    );
    return 0;
}

#[cfg(not(feature = "std"))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    // Abort only
    loop {}
}
