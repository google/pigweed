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
use core::cmp::min;
use core::ops::Range;
use core::ptr::NonNull;

use pw_status::{Error, Result};

use crate::{Kernel, MemoryRegionType};

// A buffer that resides in a process
//
// INVARIANTS:
// * All access to the underlying memory is done through the `UserBuffer`. The
//  `UserBuffer` will never expose a pointer or reference to the underlying memory.
//  Additionally `UserBuffer` is in its own leaf module to guard against other
//  code having access to its private fields.
// * Operation on the buffer always respects the access rights of the process to which
//   it is bound.
pub struct SyscallBuffer {
    addr: NonNull<u8>,
    size: usize,
    access_type: MemoryRegionType,
}

impl SyscallBuffer {
    /// Create a new buffer with access rights bound to the current process.
    pub fn new_in_current_process<K: Kernel>(
        kernel: K,
        access_type: MemoryRegionType,
        range: Range<usize>,
    ) -> Result<Self> {
        let sched = kernel.get_scheduler().lock(kernel);
        let process = sched.current_thread().process();

        // SAFETY: Since there is no dynamic memory and processes can not be
        // terminated and restarted, the access check can happen at buffer creates
        // and still uphold the invariant that the processes access rights are respected.
        //
        // TODO: https://pwbug.dev/442660183 - Handle access checks with process termination.
        if !process.range_has_access(access_type, range.clone()) {
            return Err(Error::PermissionDenied);
        }

        let Some(addr) = NonNull::new(core::ptr::with_exposed_provenance_mut(range.start)) else {
            return Err(Error::InvalidArgument);
        };
        Ok(Self {
            addr,
            size: range.len(),
            access_type,
        })
    }

    /// Returns the size of the buffer.
    #[must_use]
    pub fn size(&self) -> usize {
        self.size
    }

    /// Reduce the size of the buffer
    ///
    /// # Panics
    /// Panics if new_len is greater than the current buffer length.
    pub fn truncate(&mut self, new_size: usize) {
        pw_assert::assert!(new_size <= self.size);
        self.size = new_size;
    }

    /// Copy from this buffer into another.
    ///
    /// Copies data from this buffer into `into_buffer`, starting at `offset` bytes
    /// from the beginning of this buffer.  Will copy maximum number of bytes
    /// that exist and this buffer and will fit into `into_buffer`
    ///
    /// # Returns
    /// - Ok(0): No data was copied.  This can occur if `offset` is equal to the
    ///   size of the buffer or `into_buffer is of size zero.
    /// - Ok(len): Returns the number of bytes copied.
    /// - Error::PermissionDenied: Either this buffer is not readable or `into_buffer`
    ///   is not writeable
    /// - Error::OutOfRange: `offset` is larger than the size of the buffer.
    pub fn copy_into(&self, offset: usize, into_buffer: &mut SyscallBuffer) -> Result<usize> {
        if !self.access_type.is_readable() || !into_buffer.access_type.is_writeable() {
            return Err(Error::PermissionDenied);
        }

        if offset > self.size {
            return Err(Error::OutOfRange);
        }

        let available_bytes = self.size - offset;
        let copy_len = min(available_bytes, into_buffer.size);

        // SAFETY: The access right invariant is upheld at buffer creation time
        // where the addr and size fields are validated.
        unsafe {
            self.addr
                .byte_add(offset)
                .copy_to(into_buffer.addr, copy_len);
        }

        Ok(copy_len)
    }

    #[must_use]
    pub fn as_slice(&self) -> &[u8] {
        // Safety: Address and size are checked and validated in `new()`.
        unsafe { core::slice::from_raw_parts(self.addr.as_ptr(), self.size) }
    }
}
