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

use pw_cast::CastInto as _;
use pw_log::info;
use pw_status::{Error, Result};
use syscall_defs::{SysCallId, SysCallReturnValue};
use time::Instant;

use crate::Kernel;
use crate::object::Signals;

const SYSCALL_DEBUG: bool = false;

/// An arch-specific collection of syscall arguments
///
/// Since architectures ABI, calling, and syscall conventions can differ, this
/// trait provides a common API for extracting syscall arguments.  The API does
/// not provide any index based methods for retrieving arguments because, on some
/// architectures, the location of an argument can depend of the size of the
/// preceding arguments.
pub trait SyscallArgs<'a> {
    /// Return the next `usize` argument.
    fn next_usize(&mut self) -> Result<usize>;

    /// Return the next `u64` argument.
    fn next_u64(&mut self) -> Result<u64>;

    /// Return the next `u32` argument.
    fn next_u32(&mut self) -> Result<u32> {
        u32::try_from(self.next_usize()?).map_err(|_| Error::InvalidArgument)
    }
}

pub fn handle_syscall<'a, K: Kernel>(
    kernel: K,
    id: u16,
    mut args: K::SyscallArgs<'a>,
) -> Result<u64> {
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: {:#06x}", id as usize);

    // Instead of having a architecture independent match here, an array of
    // extern "C" function pointers could be kept and use the architecture's
    // calling convention to directly call them.
    //
    // This allows [`arch::arm_cortex_m::in_interrupt_handler()`] to treat
    // active SVCalls as not in interrupt context.
    let id: SysCallId = id.try_into()?;
    let res = match id {
        SysCallId::ObjectWait => {
            let handle = args.next_usize()?;
            let signals = args.next_usize()?;
            let deadline = args.next_u64()?;
            let Some(signal_mask) = Signals::from_bits(u32::try_from(signals).unwrap()) else {
                log_if::debug_if!(
                    SYSCALL_DEBUG,
                    "sycall: ObjectWait invalid signal mask: {:#010x}",
                    signals as usize
                );

                return Err(Error::InvalidArgument);
            };
            let deadline = Instant::<K::Clock>::from_ticks(deadline);
            log_if::debug_if!(
                SYSCALL_DEBUG,
                "sycall: ObjectWait handle {:#010x} mask {:#010x} until{:x} {:x}",
                handle as usize,
                signal_mask.bits() as usize,
                (deadline.ticks() & 0xffff_fffff) as u64,
                ((deadline.ticks() >> 32) & 0xffff_fffff) as u64,
            );
            let Some(object) = kernel
                .get_scheduler()
                .lock(kernel)
                .current_thread()
                .get_object(kernel, u32::try_from(handle).unwrap())
            else {
                log_if::debug_if!(
                    SYSCALL_DEBUG,
                    "sycall: ObjectWait can't access handle {}",
                    handle as usize
                );
                return Err(Error::OutOfRange);
            };
            let ret = object.object_wait(kernel, signal_mask, deadline).map(|_| 0);
            log_if::debug_if!(SYSCALL_DEBUG, "done");
            ret
        }
        SysCallId::DebugNoOp => Ok(0),
        SysCallId::DebugAdd => {
            let a = args.next_usize()?;
            let b = args.next_usize()?;
            log_if::debug_if!(
                SYSCALL_DEBUG,
                "syscall: DebugAdd({:#x}, {:#x}) sleeping",
                a as usize,
                b as usize,
            );
            crate::sleep_until(kernel, kernel.now() + crate::Duration::from_secs(1));
            log_if::debug_if!(SYSCALL_DEBUG, "sycall: DebugAdd woken");
            a.checked_add(b)
                .map(|res| res.cast_into())
                .ok_or(Error::OutOfRange)
        }
        // TODO: Remove this syscall when logging is added.
        SysCallId::DebugPutc => {
            log_if::debug_if!(SYSCALL_DEBUG, "sycall: sleeping");
            crate::sleep_until(kernel, kernel.now() + crate::Duration::from_secs(1));
            let arg = args.next_u32()?;
            let c = char::from_u32(arg).ok_or(Error::InvalidArgument)?;
            info!("{}", c as char);
            Ok(arg.cast_into())
        }
        // TODO: Consider adding an feature flagged PowerManager object and move
        // this shutdown call to it.
        SysCallId::DebugShutdown => {
            let exit_code = args.next_u32()?;
            log_if::debug_if!(SYSCALL_DEBUG, "sycall: Shutdown {}", exit_code as u32);
            crate::target::shutdown(exit_code);
        }
    };
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: {:#06x} returning", id as usize);
    res
}

#[allow(dead_code)]
pub fn raw_handle_syscall<'a, K: Kernel>(kernel: K, id: u16, args: K::SyscallArgs<'a>) -> i64 {
    let ret_val: SysCallReturnValue = handle_syscall(kernel, id, args).into();
    ret_val.0
}
