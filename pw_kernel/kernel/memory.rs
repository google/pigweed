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

const READABLE: usize = 1 << 0;
const WRITEABLE: usize = 1 << 1;
const EXECUTABLE: usize = 1 << 2;
const DEVICE: usize = 1 << 3;

#[derive(Clone, Copy)]
#[allow(dead_code)]
#[repr(usize)]
pub enum MemoryRegionType {
    /// Read Only, Non-Executable data.
    ReadOnlyData = READABLE,

    /// Mode Read/Write, Non-Executable data.
    ReadWriteData = READABLE | WRITEABLE,

    /// Mode Read Only, Executable data.
    ReadOnlyExecutable = READABLE | EXECUTABLE,

    /// Mode Read/Write, Executable data.
    ReadWriteExecutable = READABLE | WRITEABLE | EXECUTABLE,

    /// Device MMIO memory.
    Device = READABLE | WRITEABLE | DEVICE,
}

impl MemoryRegionType {
    #[must_use]
    pub fn has_access(&self, request: Self) -> bool {
        let request_bits = request as usize;
        let self_bits = *self as usize;
        request_bits & self_bits == request_bits
    }

    #[must_use]
    const fn is_mask_set(&self, mask: usize) -> bool {
        *self as usize & mask == mask
    }

    #[must_use]
    pub const fn is_readable(&self) -> bool {
        self.is_mask_set(READABLE)
    }

    #[must_use]
    pub const fn is_writeable(&self) -> bool {
        self.is_mask_set(WRITEABLE)
    }

    #[must_use]
    pub const fn is_executable(&self) -> bool {
        self.is_mask_set(EXECUTABLE)
    }
}

/// Architecture independent memory region description.
///
/// `MemoryRegion` provides an architecture independent way to describe a memory
/// regions and its configuration.
#[allow(dead_code)]
pub struct MemoryRegion {
    /// Type of the memory region
    pub ty: MemoryRegionType,

    /// Start address of the memory region (inclusive)
    pub start: usize,

    /// Start address of the memory region (exclusive)
    pub end: usize,
}

impl MemoryRegion {
    #[must_use]
    pub const fn new(ty: MemoryRegionType, start: usize, end: usize) -> Self {
        Self { ty, start, end }
    }

    #[must_use]
    pub fn has_access(&self, region: &Self) -> bool {
        if !(self.start..self.end).contains(&region.start)
            || !(self.start..self.end).contains(&(region.end - 1))
        {
            return false;
        }

        self.ty.has_access(region.ty)
    }

    #[must_use]
    pub fn regions_have_access(regions: &[Self], validation_region: &Self) -> bool {
        regions.iter().fold(false, |acc, region| {
            acc | region.has_access(validation_region)
        })
    }
}
/// Architecture agnostic operation on memory configuration
pub trait MemoryConfig {
    const KERNEL_THREAD_MEMORY_CONFIG: Self;

    /// Check for access to specified address range.
    ///
    /// Returns true if the memory configuration has access (as specified by
    /// `access_type` to the memory range specified by `start_addr` (inclusive)
    /// and `end_addr` (exclusive).
    #[allow(dead_code)]
    fn range_has_access(
        &self,
        access_type: MemoryRegionType,
        start_addr: usize,
        end_addr: usize,
    ) -> bool;

    /// Check for access to a specified object.
    ///
    /// Returns true if the memory configuration has access (as specified by
    /// `access_type` to the memory pointed to by `object`.
    #[allow(dead_code)]
    fn has_access<T: Sized>(&self, access_type: MemoryRegionType, object: *const T) -> bool {
        self.range_has_access(
            access_type,
            object as usize,
            object as usize + core::mem::size_of::<T>(),
        )
    }
}
