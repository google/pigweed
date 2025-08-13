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

use kernel::memory::{MemoryRegion, MemoryRegionType};
use kernel_config::{CortexMKernelConfigInterface as _, KernelConfig};

use crate::regs::Regs;
use crate::regs::mpu::*;

#[derive(Copy, Clone)]
struct MpuRegion {
    #[allow(dead_code)]
    rbar: RbarVal,
    #[allow(dead_code)]
    rlar: RlarVal,
}

#[repr(u8)]
enum AttrIndex {
    NormalMemoryRO = 0,
    NormalMemoryRW = 1,
    DeviceMemory = 2,
}

impl MpuRegion {
    const fn const_default() -> Self {
        Self {
            rbar: RbarVal::const_default(),
            rlar: RlarVal::const_default(),
        }
    }

    const fn from_memory_region(region: &MemoryRegion) -> Self {
        // TODO: handle unaligned regions.  Fail/Panic?
        let (xn, sh, ap, attr_index) = match region.ty {
            MemoryRegionType::ReadOnlyData => (
                /* xn */ true,
                RbarSh::OuterShareable,
                RbarAp::RoAny,
                AttrIndex::NormalMemoryRO,
            ),
            MemoryRegionType::ReadWriteData => {
                (
                    /* xn */ true,
                    RbarSh::NonShareable,
                    RbarAp::RwAny,
                    AttrIndex::NormalMemoryRW,
                )
            }
            MemoryRegionType::ReadOnlyExecutable => {
                (
                    /* xn */ false,
                    RbarSh::OuterShareable,
                    RbarAp::RoAny,
                    AttrIndex::NormalMemoryRO,
                )
            }
            MemoryRegionType::ReadWriteExecutable => {
                (
                    /* xn */ false,
                    RbarSh::OuterShareable,
                    RbarAp::RwAny,
                    AttrIndex::NormalMemoryRW,
                )
            }
            MemoryRegionType::Device => {
                (
                    /* xn */ true,
                    RbarSh::OuterShareable,
                    RbarAp::RoAny,
                    AttrIndex::DeviceMemory,
                )
            }
        };

        Self {
            rbar: RbarVal::const_default()
                .with_xn(xn)
                .with_sh(sh)
                .with_ap(ap)
                .with_base(region.start as u32),

            rlar: RlarVal::const_default()
                .with_en(true)
                .with_attrindx(attr_index as u8)
                .with_pxn(false)
                .with_limit(region.end as u32),
        }
    }
}

/// Cortex-M memory configuration
///
/// Represents the full configuration of the Cortex-M memory configuration
/// through the MPU block.
pub struct MemoryConfig {
    mpu_regions: [MpuRegion; KernelConfig::NUM_MPU_REGIONS],
    generic_regions: &'static [MemoryRegion],
}

impl MemoryConfig {
    /// Create a new `MemoryConfig` in a `const` context
    ///
    /// # Panics
    /// Will panic if the current target's MPU does not support enough regions
    /// to represent `regions`.
    pub const fn const_new(regions: &'static [MemoryRegion]) -> Self {
        let mut mpu_regions = [MpuRegion::const_default(); KernelConfig::NUM_MPU_REGIONS];
        let mut i = 0;
        while i < regions.len() {
            mpu_regions[i] = MpuRegion::from_memory_region(&regions[i]);
            i += 1;
        }
        Self {
            mpu_regions,
            generic_regions: regions,
        }
    }

    /// Write this memory configuration to the MPU registers.
    pub unsafe fn write(&self) {
        let mut mpu = Regs::get().mpu;
        mpu.ctrl.write(
            mpu.ctrl
                .read()
                .with_enable(false)
                .with_hfnmiena(false)
                .with_privdefena(true),
        );
        for (index, region) in self.mpu_regions.iter().enumerate() {
            mpu.rnr.write(RnrVal::default().with_region(index as u8));
            mpu.rbar.write(region.rbar);
            mpu.rlar.write(region.rlar);
        }
        mpu.ctrl.write(mpu.ctrl.read().with_enable(true));
    }

    /// Log the details of the memory configuration.
    pub fn dump(&self) {
        for (index, region) in self.mpu_regions.iter().enumerate() {
            pw_log::debug!(
                "{}: {:#010x} {:#010x}",
                index as usize,
                region.rbar.0 as usize,
                region.rlar.0 as usize
            );
        }
    }
}

/// Initialize the MPU for supporting user space memory protection.
pub fn init() {
    let mut mpu = Regs::get().mpu;

    // Attributes below are the recommended values from
    // https://developer.arm.com/documentation/107565/0101/Memory-protection/Memory-Protection-Unit
    let val = Mair0Val::default()
        // AttrIndex::NormalMemoryRO
        .with_attr0(MairAttr::normal_memory(
            MairNormalMemoryCaching::WriteBackNonTransientRO,
            MairNormalMemoryCaching::WriteBackNonTransientRO,
        ))
        // AttrIndex::NormalMemoryRW
        .with_attr1(MairAttr::normal_memory(
            MairNormalMemoryCaching::WriteBackNonTransientRW,
            MairNormalMemoryCaching::WriteBackNonTransientRW,
        ))
        // AttrIndex::DeviceMemory
        .with_attr2(MairAttr::device_memory(MairDeviceMemoryOrdering::nGnRE));
    mpu.mair0.write(val);
}

impl kernel::memory::MemoryConfig for MemoryConfig {
    const KERNEL_THREAD_MEMORY_CONFIG: Self = Self::const_new(&[MemoryRegion::new(
        MemoryRegionType::ReadWriteExecutable,
        0x0000_0000,
        0xffff_ffff,
    )]);

    fn range_has_access(
        &self,
        access_type: MemoryRegionType,
        start_addr: usize,
        end_addr: usize,
    ) -> bool {
        let validation_region = MemoryRegion::new(access_type, start_addr, end_addr);
        MemoryRegion::regions_have_access(&self.generic_regions, &validation_region)
    }
}
