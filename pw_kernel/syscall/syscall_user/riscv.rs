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

use core::arch::naked_asm;

use pw_status::Result;
use syscall_defs::{SysCallId, SysCallInterface, SysCallReturnValue};

pub struct SysCall {}

macro_rules! syscall_veneer {
    ($id:ident, $name:ident($($arg_name:ident: $arg_type:ty),* $(,)?)) => {
        #[unsafe(naked)]
        unsafe extern "C" fn $name($($arg_name: $arg_type),*) -> i64 {
            naked_asm!("
                li   t0, {id}
                ecall
                ret
                ",
                id = const SysCallId::$id as u32
            );
        }
    };
}

syscall_veneer!(ObjectWait, object_wait(handle: u32, signals: u32, deadline: u64));
syscall_veneer!(ChannelTransact, channel_transact(
    object_handle: u32, // a0
    send_data: *mut u8, // a1
    send_len: usize,    // a2
    recv_data: *mut u8, // a3
    recv_len: usize,    // a4
    deadline: u64,      // a6-a7
));
syscall_veneer!(ChannelRead, channel_read(
     object_handle: u32,
     offset: usize,
     buffer: *mut u8,
     buffer_len: usize,
));
syscall_veneer!(ChannelRespond, channel_respond(object_handle: u32, buffer: *mut u8, buffer_len: usize));
syscall_veneer!(DebugPutc, putc(a: u32));
syscall_veneer!(DebugShutdown, shutdown(a: u32));

impl SysCallInterface for SysCall {
    #[inline(always)]
    fn object_wait(handle: u32, signals: u32, deadline: u64) -> Result<()> {
        SysCallReturnValue(unsafe { object_wait(handle, signals, deadline) }).to_result_unit()
    }

    #[inline(always)]
    fn channel_transact(
        handle: u32,
        send_data: *mut u8,
        send_len: usize,
        recv_data: *mut u8,
        recv_len: usize,
        deadline: u64,
    ) -> Result<u32> {
        SysCallReturnValue(unsafe {
            channel_transact(handle, send_data, send_len, recv_data, recv_len, deadline)
        })
        .to_result_u32()
    }

    #[inline(always)]
    fn channel_read(handle: u32, offset: usize, buffer: *mut u8, buffer_len: usize) -> Result<u32> {
        SysCallReturnValue(unsafe { channel_read(handle, offset, buffer, buffer_len) })
            .to_result_u32()
    }

    #[inline(always)]
    fn channel_respond(handle: u32, buffer: *mut u8, buffer_len: usize) -> Result<()> {
        SysCallReturnValue(unsafe { channel_respond(handle, buffer, buffer_len) }).to_result_unit()
    }

    #[inline(always)]
    fn debug_putc(a: u32) -> Result<u32> {
        SysCallReturnValue(unsafe { putc(a) }).to_result_u32()
    }

    #[inline(always)]
    fn debug_shutdown(a: u32) -> Result<()> {
        SysCallReturnValue(unsafe { shutdown(a) }).to_result_unit()
    }
}
