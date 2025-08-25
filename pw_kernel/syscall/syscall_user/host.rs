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
use syscall_defs::SysCallInterface;

pub struct SysCall {}

impl SysCallInterface for SysCall {
    #[inline(always)]
    fn object_wait(_: u32, _: u32, _: u64) -> Result<()> {
        Err(pw_status::Error::Unimplemented)
    }

    #[inline(always)]
    fn debug_noop() -> Result<()> {
        Err(pw_status::Error::Unimplemented)
    }

    #[inline(always)]
    fn debug_add(_a: u32, _b: u32) -> Result<u32> {
        Err(pw_status::Error::Unimplemented)
    }

    #[inline(always)]
    fn debug_putc(_a: u32) -> Result<u32> {
        Err(pw_status::Error::Unimplemented)
    }

    #[inline(always)]
    fn debug_shutdown(_a: u32) -> Result<()> {
        Err(pw_status::Error::Unimplemented)
    }
}
