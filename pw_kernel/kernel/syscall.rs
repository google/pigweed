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

use pw_status::Result;

use syscall_defs::{SysCallId, SysCallReturnValue};

pub fn handle_syscall(
    id: u16,
    arg0: usize,
    arg1: usize,
    _arg2: usize,
    _arg3: usize,
) -> Result<u64> {
    // Instead of having a architecture independent match here, an array of
    // extern "C" function pointers could be kept and use the architecture's
    // calling convention to directly call them.
    let id: SysCallId = id.try_into()?;
    match id {
        SysCallId::DebugNoOp => Ok(0),
        SysCallId::DebugAdd => Ok((arg0 + arg1) as u64),
    }
}

#[allow(dead_code)]
pub fn raw_handle_syscall(id: u16, arg0: usize, arg1: usize, arg2: usize, arg3: usize) -> i64 {
    let ret_val: SysCallReturnValue = handle_syscall(id, arg0, arg1, arg2, arg3).into();
    ret_val.0
}
