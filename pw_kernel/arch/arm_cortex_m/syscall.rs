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

use kernel::syscall::raw_handle_syscall;

use super::exceptions::{exception, KernelExceptionFrame};
use super::regs::Regs;

// Pulls arguments out of the exception frame and calls the arch-independent
// syscall handler.
//
// There is potential to put the call to `raw_handle_syscall()` directly into
// the SVCall handler to save some code space and execution time.
#[exception(exception = "SVCall")]
#[no_mangle]
extern "C" fn handle_svc(frame: *mut KernelExceptionFrame) -> *mut KernelExceptionFrame {
    // In order to allow syscalls to block, be preempted, and have other
    // threads make syscalls they need be run without svcall being active.  If
    // this is not done and the system switches to a thread which makes another
    // system call, a hard fault will occur.
    let mut scb = Regs::get().scb;
    let val = scb.shcsr.read().with_svcall_act(false);
    scb.shcsr.write(val);

    let ret_val = raw_handle_syscall(
        super::Arch,
        unsafe { &*frame }.r11 as u16,
        unsafe { &*frame }.r4 as usize,
        unsafe { &*frame }.r5 as usize,
        unsafe { &*frame }.r6 as usize,
        unsafe { &*frame }.r7 as usize,
    );
    unsafe { &mut *frame }.r4 = (ret_val as u64) as u32;
    unsafe { &mut *frame }.r5 = (ret_val as u64 >> 32) as u32;

    // In order for the return from svcall to return correctly, svcall needs
    // to be marked as active again.
    let val = scb.shcsr.read().with_svcall_act(true);
    scb.shcsr.write(val);

    frame
}
