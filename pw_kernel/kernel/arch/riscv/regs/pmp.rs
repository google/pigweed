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
#![allow(dead_code)]

use core::arch::asm;

use pw_status::{Error, Result};
use regs::*;

use crate::arch::{MemoryRegion, MemoryRegionType};

#[repr(u8)]
pub enum PmpCfgAddressMode {
    /// Null region (disabled)
    Off = 0,

    /// Top of Range
    Tor = 1,

    /// Naturally aligned four-byte region
    Na4 = 2,

    /// Naturally aligned power-of-two region, ≥8 bytes
    Napot = 3,
}

#[repr(transparent)]
pub struct PmpCfgVal(u8);
impl PmpCfgVal {
    rw_bool_field!(u8, r, 0, "readable");
    rw_bool_field!(u8, w, 1, "writable");
    rw_bool_field!(u8, x, 2, "executable");
    rw_enum_field!(u8, a, 3, 4, PmpCfgAddressMode, "addressing mode");
    rw_bool_field!(u8, l, 7, "locked");
}

impl PmpCfgVal {
    pub const fn from_region_type(ty: MemoryRegionType, address_mode: PmpCfgAddressMode) -> Self {
        match ty {
            MemoryRegionType::ReadOnlyData => Self(0)
                .with_r(true)
                .with_w(false)
                .with_x(false)
                .with_a(address_mode),
            MemoryRegionType::Device | MemoryRegionType::ReadWriteData => Self(0)
                .with_r(true)
                .with_w(true)
                .with_x(false)
                .with_a(address_mode),
            MemoryRegionType::ReadOnlyExecutable => Self(0)
                .with_r(true)
                .with_w(false)
                .with_x(true)
                .with_a(address_mode),
            MemoryRegionType::ReadWriteExecutable => Self(0)
                .with_r(true)
                .with_w(true)
                .with_x(true)
                .with_a(address_mode),
        }
    }
}

#[derive(Clone)]
pub struct PmpConfig<const NUM_CFG_REGISTERS: usize, const NUM_ENTRIES: usize> {
    pub cfg: [usize; NUM_CFG_REGISTERS],
    pub addr: [usize; NUM_ENTRIES],
}

impl<const NUM_CFG_REGISTERS: usize, const NUM_ENTRIES: usize>
    PmpConfig<NUM_CFG_REGISTERS, NUM_ENTRIES>
{
    /// Creates a new PMP configuration representing the provided `regions`.
    ///
    /// Since the PMP's[^pmp] configuration can take either one or two entries
    /// to represent a regions depending on whether or not two regions are
    /// back-to-back one or two PMP entries may be used.
    ///
    /// [^pmp]: Section 3.7. Physical Memory Protection in
    ///   [The RISC-V Instruction Set Manual Volume II: Privileged Architecture](https://github.com/riscv/riscv-isa-manual/releases/download/20250508/riscv-privileged-20250508.pdf)
    pub const fn new(regions: &[MemoryRegion]) -> Result<Self> {
        let mut cfg = Self {
            cfg: [0; NUM_CFG_REGISTERS],
            addr: [0; NUM_ENTRIES],
        };
        let mut cur_region = 0;
        let mut cur_entry = 0;

        // The iteration of `regions` is somewhat awkwardly done using a `while`
        // loop instead of a `for` loop because `for` loops are not supported
        // in const functions.
        while cur_region < regions.len() {
            let region = &regions[cur_region];

            if let Err(e) = cfg.entry(
                cur_entry,
                PmpCfgVal::from_region_type(region.ty, PmpCfgAddressMode::Na4),
                region.start >> 2,
            ) {
                return Err(e);
            }
            cur_region += 1;
            cur_entry += 1;

            // When two regions are back-to-back marking the end of the first
            // regions with a Top of Range entry is not required.
            if cur_region < regions.len() && regions[cur_region].start == region.end {
                continue;
            }

            // When two regions are not back-to-back, the end of a region needs
            // to be marked with a Top of Range (TOR) entry.
            if let Err(e) = cfg.entry(
                cur_entry,
                PmpCfgVal::from_region_type(region.ty, PmpCfgAddressMode::Tor),
                region.end >> 2,
            ) {
                return Err(e);
            }
            cur_entry += 1;
        }

        Ok(cfg)
    }

    const fn configs_per_register() -> usize {
        NUM_ENTRIES / NUM_CFG_REGISTERS
    }

    pub const fn entry(
        &mut self,
        index: usize,
        config: PmpCfgVal,
        address: usize,
    ) -> Result<&mut Self> {
        if index >= NUM_ENTRIES {
            return Err(Error::ResourceExhausted);
        }

        let cfg_index = index / Self::configs_per_register();
        let cfg_shift = (index % Self::configs_per_register()) * 8;

        let mut cfg_val = self.cfg[cfg_index];
        cfg_val &= !(0xff << cfg_shift);
        cfg_val |= (config.0 as usize) << cfg_shift;
        self.cfg[cfg_index] = cfg_val;

        self.addr[index] = address;

        Ok(self)
    }

    /// Write this PMP configuration to the registers.
    pub unsafe fn write(&self) {
        // Currently only 16 entry, rv32 PMPs are supported.
        pw_assert::debug_assert!(NUM_ENTRIES == 16);
        pw_assert::debug_assert!((NUM_ENTRIES / 4) == NUM_CFG_REGISTERS);

        unsafe {
            asm!("csrw pmpcfg0, {cfg}", cfg = in(reg) self.cfg[0]);
            asm!("csrw pmpcfg1, {cfg}", cfg = in(reg) self.cfg[1]);
            asm!("csrw pmpcfg2, {cfg}", cfg = in(reg) self.cfg[2]);
            asm!("csrw pmpcfg3, {cfg}", cfg = in(reg) self.cfg[3]);

            asm!("csrw pmpaddr0, {addr}", addr = in(reg) self.addr[0]);
            asm!("csrw pmpaddr1, {addr}", addr = in(reg) self.addr[1]);
            asm!("csrw pmpaddr2, {addr}", addr = in(reg) self.addr[2]);
            asm!("csrw pmpaddr3, {addr}", addr = in(reg) self.addr[3]);
            asm!("csrw pmpaddr4, {addr}", addr = in(reg) self.addr[4]);
            asm!("csrw pmpaddr5, {addr}", addr = in(reg) self.addr[5]);
            asm!("csrw pmpaddr6, {addr}", addr = in(reg) self.addr[6]);
            asm!("csrw pmpaddr7, {addr}", addr = in(reg) self.addr[7]);
            asm!("csrw pmpaddr8, {addr}", addr = in(reg) self.addr[8]);
            asm!("csrw pmpaddr9, {addr}", addr = in(reg) self.addr[9]);
            asm!("csrw pmpaddr10, {addr}", addr = in(reg) self.addr[10]);
            asm!("csrw pmpaddr11, {addr}", addr = in(reg) self.addr[11]);
            asm!("csrw pmpaddr12, {addr}", addr = in(reg) self.addr[12]);
            asm!("csrw pmpaddr13, {addr}", addr = in(reg) self.addr[13]);
            asm!("csrw pmpaddr14, {addr}", addr = in(reg) self.addr[14]);
            asm!("csrw pmpaddr15, {addr}", addr = in(reg) self.addr[15]);
        }
    }

    /// Log the details of the PMP configuration.
    pub fn dump(&self) {
        for (i, cfg) in self.cfg.iter().enumerate() {
            pw_log::debug!("pmpcfg{}: {:#10x}", i as usize, *cfg as usize);
        }

        for (i, addr) in self.addr.iter().enumerate() {
            pw_log::debug!("pmpaddr{}: {:#10x}", i as usize, *addr as usize);
        }
    }
}
