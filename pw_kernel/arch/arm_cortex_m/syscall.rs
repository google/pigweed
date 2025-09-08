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

use kernel::SyscallArgs;
use kernel::syscall::raw_handle_syscall;
use pw_cast::CastInto as _;
use pw_status::{Error, Result};

use super::exceptions::{KernelExceptionFrame, exception};
use super::regs::Regs;

pub struct CortexMSyscallArgs<'a> {
    frame: &'a KernelExceptionFrame,
    cur_index: usize,
}

impl<'a> CortexMSyscallArgs<'a> {
    fn new(frame: &'a KernelExceptionFrame) -> Self {
        Self {
            frame,
            cur_index: 0,
        }
    }
}

impl<'a> SyscallArgs<'a> for CortexMSyscallArgs<'a> {
    fn next_usize(&mut self) -> Result<usize> {
        if self.cur_index > 6 {
            return Err(Error::InvalidArgument);
        }
        // Pointer math is used here instead of a match statement as it results
        // in significantly smaller code.
        //
        // SAFETY: Index is range checked above and the r* fields are in consecutive
        // positions in the `KernelExceptionFrame` struct.
        let usize0 = &raw const self.frame.r4;
        let value = unsafe { *usize0.byte_add(self.cur_index * 4) };
        self.cur_index += 1;
        Ok(value.cast_into())
    }

    #[inline(always)]
    fn next_u64(&mut self) -> Result<u64> {
        let low: u64 = self.next_usize()?.cast_into();
        let high: u64 = self.next_usize()?.cast_into();
        Ok(low | high << 32)
    }
}

// Pulls arguments out of the exception frame and calls the arch-independent
// syscall handler.
//
// There is potential to put the call to `raw_handle_syscall()` directly into
// the SVCall handler to save some code space and execution time.
#[exception(exception = "SVCall")]
#[unsafe(no_mangle)]
extern "C" fn handle_svc(frame_ptr: *mut KernelExceptionFrame) -> *mut KernelExceptionFrame {
    // In order to allow syscalls to block, be preempted, and have other
    // threads make syscalls they need be run without svcall being active.  If
    // this is not done and the system switches to a thread which makes another
    // system call, a hard fault will occur.
    let mut scb = Regs::get().scb;
    let val = scb.shcsr.read().with_svcall_act(false);
    scb.shcsr.write(val);

    let frame = unsafe { &mut *frame_ptr };

    let id = frame.r11 as u16;
    let args = CortexMSyscallArgs::new(frame);
    let ret_val = raw_handle_syscall(super::Arch, id, args);
    frame.r4 = (ret_val as u64) as u32;
    frame.r5 = (ret_val as u64 >> 32) as u32;

    // In order for the return from svcall to return correctly, svcall needs
    // to be marked as active again.
    let val = scb.shcsr.read().with_svcall_act(true);
    scb.shcsr.write(val);

    frame_ptr
}
