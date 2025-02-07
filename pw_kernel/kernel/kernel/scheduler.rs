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

use core::mem::offset_of;

use super::{Arch, ArchInterface};
use list::Link;

pub struct Stack {
    #[allow(dead_code)]
    start: *const u8,

    #[allow(dead_code)]
    end: *const u8,
}

impl Stack {
    pub const fn from_slice(slice: &[u8]) -> Self {
        let start: *const u8 = slice.as_ptr();
        // Safety: offset based on known size of slice.
        let end = unsafe { start.add(slice.len() - 1) };
        Self { start, end }
    }
}

pub struct Thread {
    pub link: Link,

    #[allow(dead_code)]
    pub thread_state: <Arch as ArchInterface>::ThreadState,

    #[allow(dead_code)]
    pub stack: Stack,
}

#[allow(dead_code)]
pub struct ThreadListAdapter {}

impl list::Adapter for ThreadListAdapter {
    const LINK_OFFSET: usize = offset_of!(Thread, link);
}
