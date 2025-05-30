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

use kernel_config::{KernelConfig, RiscVKernelConfigInterface};

use crate::arch::riscv::regs::pmp::*;
use crate::arch::{MemoryRegion, MemoryRegionType};

/// RISC-V memory configuration
///
/// Represents the full configuration of RISC-V memory configuration through
/// the PMP block.
#[derive(Clone)]
pub struct MemoryConfig(
    PmpConfig<{ KernelConfig::PMP_CFG_REGISTERS }, { KernelConfig::PMP_ENTRIES }>,
);

impl MemoryConfig {
    pub const KERNEL_THREAD_MEMORY_CONFIG: Self = Self::const_new(&[MemoryRegion {
        ty: MemoryRegionType::ReadWriteExecutable,
        start: 0x0000_0000,
        end: 0xffff_ffff,
    }]);

    /// Create a new `MemoryConfig` in a `const` context
    ///
    /// # Panics
    /// Will panic if the current target's PMP will does not have enough entries
    /// to represent the provided `regions`.
    pub const fn const_new(regions: &[MemoryRegion]) -> Self {
        match PmpConfig::new(regions) {
            Ok(cfg) => Self(cfg),
            Err(_) => panic!("Can't create Memory config"),
        }
    }

    /// Write this memory configuration to the PMP registers.
    pub unsafe fn write(&self) {
        unsafe { self.0.write() }
    }

    /// Log the details of the memory configuration.
    pub fn dump(&self) {
        self.0.dump()
    }
}

impl crate::arch::MemoryConfig for MemoryConfig {
    fn range_has_access(
        &self,
        _access_type: MemoryRegionType,
        _start_addr: usize,
        _end_addr: usize,
    ) -> bool {
        // TODO: konkers - Implement
        true
    }
}
