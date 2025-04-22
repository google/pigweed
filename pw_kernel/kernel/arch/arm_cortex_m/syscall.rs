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

use crate::syscall::raw_handle_syscall;

use super::exceptions::{exception, KernelExceptionFrame};

// Pulls arguments out of the exception frame and calls the arch-independent
// syscall handler.
//
// There is potential to put the call to `raw_handle_syscall()` directly into
// the SVCall handler to save some code space and execution time.
#[exception(exception = "SVCall", disable_interrupts)]
#[no_mangle]
extern "C" fn handle_svc(frame: *mut KernelExceptionFrame) -> *mut KernelExceptionFrame {
    let ret_val = raw_handle_syscall(
        unsafe { &*frame }.r11 as u16,
        unsafe { &*frame }.r4 as usize,
        unsafe { &*frame }.r5 as usize,
        unsafe { &*frame }.r6 as usize,
        unsafe { &*frame }.r7 as usize,
    );
    unsafe { &mut *frame }.r4 = (ret_val as u64) as u32;
    unsafe { &mut *frame }.r5 = (ret_val as u64 >> 32) as u32;
    frame
}
