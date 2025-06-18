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

#[cfg(feature = "arch_arm_cortex_m")]
pub mod arm_cortex_m;
#[cfg(feature = "arch_arm_cortex_m")]
pub use arm_cortex_m::{protection::MemoryConfig as ArchMemoryConfig, Arch};

#[cfg(feature = "arch_riscv")]
pub mod riscv;
#[cfg(feature = "arch_riscv")]
pub use riscv::{protection::MemoryConfig as ArchMemoryConfig, Arch};

#[cfg(feature = "arch_host")]
mod host;
#[cfg(feature = "arch_host")]
pub use host::{Arch, MemoryConfig as ArchMemoryConfig};

#[derive(Clone, Copy)]
#[allow(dead_code)]
pub enum MemoryRegionType {
    /// Read Only, Non-Executable data.
    ReadOnlyData,

    /// Mode Read/Write, Non-Executable data.
    ReadWriteData,

    /// Mode Read Only, Executable data.
    ReadOnlyExecutable,

    /// Mode Read/Write, Executable data.
    ReadWriteExecutable,

    /// Device MMIO memory.
    Device,
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
