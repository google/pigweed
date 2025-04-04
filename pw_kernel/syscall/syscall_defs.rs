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
#![no_std]

use pw_status::{Error, Result};

pub struct SysCallReturnValue(pub i64);

impl SysCallReturnValue {
    pub fn to_result_unit(self) -> Result<()> {
        let value = self.0;
        if value < 0 {
            // TODO debug assert if error number is out of range
            Err(unsafe { core::mem::transmute::<u32, Error>((-value) as u32) })
        } else {
            Ok(())
        }
    }
    pub fn to_result_u32(self) -> Result<u32> {
        let value = self.0;
        if value < 0 {
            // TODO debug assert if error number is out of range
            Err(unsafe { core::mem::transmute::<u32, Error>((-value) as u32) })
        } else {
            Ok(value as u32)
        }
    }
}

impl From<Result<u64>> for SysCallReturnValue {
    fn from(value: Result<u64>) -> Self {
        match value {
            // TODO - konkers: Debug assert on high bit of value being set.
            Ok(val) => Self(val as i64),
            Err(error) => Self(-(error as i64)),
        }
    }
}

#[repr(u16)]
pub enum SysCallId {
    // System calls prefixed with 0xF000 are reserved development/debugging use.
    DebugNoOp = 0xf000,
    DebugAdd = 0xf001,
}

impl TryFrom<u16> for SysCallId {
    type Error = Error;

    fn try_from(value: u16) -> core::result::Result<Self, Error> {
        match value {
            0xf000..=0xf001 => Ok(unsafe { core::mem::transmute::<u16, SysCallId>(value) }),
            _ => Err(Error::InvalidArgument),
        }
    }
}

pub trait SysCallInterface {
    fn debug_noop() -> Result<()>;
    fn debug_add(a: u32, b: u32) -> Result<u32>;
}
