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
    ($id:ident, $name:ident($($arg_name:ident: $arg_type:ty),*)) => {
        #[naked]
        unsafe extern "C" fn $name($($arg_name: $arg_type),*) -> i64 {
            naked_asm!("
                push  {{r4-r7, r11}}
                mov   r11, {id}
                mov   r4, r0
                mov   r5, r1
                mov   r6, r2
                mov   r7, r3
                svc   0
                mov   r0, r4
                mov   r1, r5
                pop  {{r4-r7, r11}}
                bx lr
                ",
                id = const SysCallId::$id as u32
            );
        }
    };
}

syscall_veneer!(DebugNoOp, noop());
syscall_veneer!(DebugAdd, add(a: u32, b: u32));
syscall_veneer!(DebugPutc, putc(a: u32));

impl SysCallInterface for SysCall {
    #[inline(always)]
    fn debug_noop() -> Result<()> {
        SysCallReturnValue(unsafe { noop() }).to_result_unit()
    }

    #[inline(always)]
    fn debug_add(a: u32, b: u32) -> Result<u32> {
        SysCallReturnValue(unsafe { add(a, b) }).to_result_u32()
    }

    #[inline(always)]
    fn debug_putc(a: u32) -> Result<u32> {
        SysCallReturnValue(unsafe { putc(a) }).to_result_u32()
    }
}
