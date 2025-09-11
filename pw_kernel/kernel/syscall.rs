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

use foreign_box::ForeignRc;
use pw_cast::CastInto as _;
use pw_log::info;
use pw_status::{Error, Result};
use syscall_defs::{SysCallId, SysCallReturnValue};
use time::{Clock, Instant};

use crate::Kernel;
use crate::object::{KernelObject, Signals, SyscallBuffer};

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
    #[inline(always)]
    fn next_u32(&mut self) -> Result<u32> {
        u32::try_from(self.next_usize()?).map_err(|_| Error::InvalidArgument)
    }

    /// Return the next `Instant` argument.
    #[inline(always)]
    fn next_instant<C: Clock>(&mut self) -> Result<Instant<C>> {
        Ok(Instant::from_ticks(self.next_u64()?))
    }
}

pub fn lookup_handle<K: Kernel>(
    kernel: K,
    handle: u32,
) -> Result<ForeignRc<K::AtomicUsize, dyn KernelObject<K>>> {
    let Some(object) = kernel
        .get_scheduler()
        .lock(kernel)
        .current_thread()
        .get_object(kernel, handle)
    else {
        log_if::debug_if!(
            SYSCALL_DEBUG,
            "sycall: ObjectWait can't access handle {}",
            handle as usize
        );
        return Err(Error::OutOfRange);
    };

    Ok(object)
}

fn handle_object_wait<'a, K: Kernel>(kernel: K, mut args: K::SyscallArgs<'a>) -> Result<u64> {
    let handle = args.next_u32()?;
    let signals = args.next_usize()?;
    let deadline = args.next_instant()?;
    log_if::debug_if!(
        SYSCALL_DEBUG,
        "syscall: handling object_wait {:x} {:x} {:x}",
        handle as u32,
        signals as usize,
        deadline.ticks() as u64
    );

    let object = lookup_handle(kernel, handle)?;
    let Some(signal_mask) = Signals::from_bits(u32::try_from(signals).unwrap()) else {
        log_if::debug_if!(
            SYSCALL_DEBUG,
            "syscall: ObjectWait invalid signal mask: {:#010x}",
            signals as usize
        );

        return Err(Error::InvalidArgument);
    };

    let ret = object.object_wait(kernel, signal_mask, deadline);
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: object_wait complete");
    ret.map(|_| 0)
}

fn handle_channel_transact<'a, K: Kernel>(kernel: K, mut args: K::SyscallArgs<'a>) -> Result<u64> {
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: handling channel_transact");
    let handle = args.next_u32()?;
    let send_data_addr = args.next_usize()?;
    let send_data_len = args.next_usize()?;
    let recv_data_addr = args.next_usize()?;
    let recv_data_len = args.next_usize()?;
    let deadline: Instant<K::Clock> = args.next_instant()?;

    log_if::debug_if!(
        SYSCALL_DEBUG,
        "syscall: handling channel_transact({:#x}, {:#x}, {:#x}, {:#x}, {:#x}, {:#x})",
        handle as u32,
        send_data_addr as usize,
        send_data_len as usize,
        recv_data_addr as usize,
        recv_data_len as usize,
        deadline.ticks() as u64,
    );

    let object = lookup_handle(kernel, handle)?;
    let send_buffer = SyscallBuffer::new_in_current_process(
        kernel,
        crate::MemoryRegionType::ReadOnlyData,
        send_data_addr..(send_data_addr + send_data_len),
    )?;
    let recv_buffer = SyscallBuffer::new_in_current_process(
        kernel,
        crate::MemoryRegionType::ReadWriteData,
        recv_data_addr..(recv_data_addr + recv_data_len),
    )?;

    let ret = object.channel_transact(kernel, send_buffer, recv_buffer, deadline);

    log_if::debug_if!(SYSCALL_DEBUG, "syscall: channel_transact complete");

    ret.map(|v| v.cast_into())
}

fn handle_channel_read<'a, K: Kernel>(kernel: K, mut args: K::SyscallArgs<'a>) -> Result<u64> {
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: handling channel_read");
    let handle = args.next_u32()?;
    let offset = args.next_usize()?;
    let buffer_addr = args.next_usize()?;
    let buffer_len = args.next_usize()?;

    let object = lookup_handle(kernel, handle)?;
    let buffer = SyscallBuffer::new_in_current_process(
        kernel,
        crate::MemoryRegionType::ReadWriteData,
        buffer_addr..(buffer_addr + buffer_len),
    )?;

    let ret = object.channel_read(kernel, offset, buffer);
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: channel_read complete");
    ret.map(|v| v.cast_into())
}

fn handle_channel_respond<'a, K: Kernel>(kernel: K, mut args: K::SyscallArgs<'a>) -> Result<u64> {
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: handling channel_respond");
    let handle = args.next_u32()?;
    let buffer_addr = args.next_usize()?;
    let buffer_len = args.next_usize()?;

    let object = lookup_handle(kernel, handle)?;
    let buffer = SyscallBuffer::new_in_current_process(
        kernel,
        crate::MemoryRegionType::ReadOnlyData,
        buffer_addr..(buffer_addr + buffer_len),
    )?;

    let ret = object.channel_respond(kernel, buffer);
    log_if::debug_if!(SYSCALL_DEBUG, "syscall: channel_respond complete");
    ret.map(|_| 0)
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
    let id: SysCallId = id.into();
    let res = match id {
        SysCallId::ObjectWait => handle_object_wait(kernel, args),
        SysCallId::ChannelTransact => handle_channel_transact(kernel, args),
        SysCallId::ChannelRead => handle_channel_read(kernel, args),
        SysCallId::ChannelRespond => handle_channel_respond(kernel, args),
        // TODO: Remove this syscall when logging is added.
        SysCallId::DebugPutc => {
            let arg = args.next_u32()?;
            let c = char::from_u32(arg).ok_or(Error::InvalidArgument)?;
            let sched = kernel.get_scheduler().lock(kernel);
            info!("{}: {}", sched.current_thread().name as &str, c as char);
            Ok(arg.cast_into())
        }
        // TODO: Consider adding an feature flagged PowerManager object and move
        // this shutdown call to it.
        SysCallId::DebugShutdown => {
            let exit_code = args.next_u32()?;
            log_if::debug_if!(SYSCALL_DEBUG, "sycall: Shutdown {}", exit_code as u32);
            crate::target::shutdown(exit_code);
        }
        _ => {
            log_if::debug_if!(SYSCALL_DEBUG, "syscall: Unknown id {}", id as usize);
            Err(Error::InvalidArgument)
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
