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

#[panic_handler]
fn panic_handler(info: &core::panic::PanicInfo) -> ! {
    if let Some(location) = info.location() {
        panic_is_possible(
            location.file().as_ptr(),
            location.file().len(),
            location.line(),
            location.column(),
        );
    } else {
        panic_is_possible(core::ptr::null(), 0, 0, 0);
    }
}

// This panic_is_possible function is the hook used by the panic detector
// to identify the presence a panic handler.
#[unsafe(no_mangle)]
#[inline(never)]
extern "C" fn panic_is_possible(
    filename: *const u8,
    filename_len: usize,
    line: u32,
    col: u32,
) -> ! {
    // The arguments to this function are reverse-engineered
    // from the machine code by static analysis to
    // display a list of all the panic call-sites to the
    // user in the rust_binary_no_panics_test error message.
    // See pw_kernel/tooling/panic_detector/check_panic.rs for more details.

    // If this symbol exists in the binary, panics are
    // possible. Presubmit tests can ensure that this symbol
    // does not exist in the final binary.  Do not rename or
    // remove this function.
    core::hint::black_box(filename);
    core::hint::black_box(filename_len);
    core::hint::black_box(line);
    core::hint::black_box(col);
    loop {}
}
