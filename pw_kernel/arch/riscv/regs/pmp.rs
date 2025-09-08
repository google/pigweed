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

#[cfg(not(feature = "epmp"))]
use kernel::MemoryRegionType;
use kernel::memory::MemoryRegion;
use kernel_config::{KernelConfig, RiscVKernelConfigInterface as _};
use pw_status::{Error, Result};
use regs::*;

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
#[derive(Clone, Copy)]
pub struct PmpCfgVal(pub(crate) u8);
impl PmpCfgVal {
    rw_bool_field!(u8, r, 0, "readable");
    rw_bool_field!(u8, w, 1, "writable");
    rw_bool_field!(u8, x, 2, "executable");
    rw_enum_field!(u8, a, 3, 4, PmpCfgAddressMode, "addressing mode");
    rw_bool_field!(u8, l, 7, "locked");
}

impl Default for PmpCfgVal {
    fn default() -> Self {
        PmpCfgVal(0)
    }
}

impl PmpCfgVal {
    #[cfg(not(feature = "epmp"))]
    pub const fn from_region_type(ty: MemoryRegionType, address_mode: PmpCfgAddressMode) -> Self {
        // Prepare PmpCfgVals for PMP.
        Self(0)
            .with_r(ty.is_readable())
            .with_w(ty.is_writeable())
            .with_x(ty.is_executable())
            .with_a(address_mode)
    }
}

#[derive(Clone)]
#[repr(align(4))]
pub struct PmpConfig<const NUM_ENTRIES: usize> {
    pub cfg: [PmpCfgVal; NUM_ENTRIES],
    pub addr: [usize; NUM_ENTRIES],
}

impl<const NUM_ENTRIES: usize> PmpConfig<NUM_ENTRIES> {
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
            cfg: [PmpCfgVal(0); NUM_ENTRIES],
            addr: [0; NUM_ENTRIES],
        };
        let mut cur_region = 0;
        let mut cur_entry = KernelConfig::PMP_USERSPACE_ENTRIES.start;

        // The iteration of `regions` is somewhat awkwardly done using a `while`
        // loop instead of a `for` loop because `for` loops are not supported
        // in const functions.
        while cur_region < regions.len() {
            if cur_entry >= KernelConfig::PMP_USERSPACE_ENTRIES.end {
                return Err(Error::ResourceExhausted);
            }
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
            if cur_entry >= KernelConfig::PMP_USERSPACE_ENTRIES.end {
                return Err(Error::ResourceExhausted);
            }

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

    pub const fn entry(
        &mut self,
        index: usize,
        config: PmpCfgVal,
        address: usize,
    ) -> Result<&mut Self> {
        if index >= NUM_ENTRIES {
            return Err(Error::ResourceExhausted);
        }
        self.cfg[index] = config;
        self.addr[index] = address;
        Ok(self)
    }

    /// Clear the PMPCFG registers disabling the current configuration.
    pub unsafe fn clear(&self) {
        unsafe {
            asm!("csrw pmpcfg0, x0");
            asm!("csrw pmpcfg1, x0");
            asm!("csrw pmpcfg2, x0");
            asm!("csrw pmpcfg3, x0");
        }
    }

    /// Write this PMP configuration to the registers.
    pub unsafe fn write(&self) {
        // Currently only 16 entry, rv32 PMPs are supported.
        pw_assert::debug_assert!(NUM_ENTRIES == 16);

        unsafe {
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

            // TODO: use zerocopy to perform this transmute.
            let config: &[usize; 4] = core::mem::transmute(&self.cfg);
            asm!("csrw pmpcfg0, {cfg}", cfg = in(reg) config[0]);
            asm!("csrw pmpcfg1, {cfg}", cfg = in(reg) config[1]);
            asm!("csrw pmpcfg2, {cfg}", cfg = in(reg) config[2]);
            asm!("csrw pmpcfg3, {cfg}", cfg = in(reg) config[3]);
        }
    }

    /// Read the PMP configuration from the registers.
    pub unsafe fn read() -> Self {
        // Currently only 16 entry, rv32 PMPs are supported.
        pw_assert::debug_assert!(NUM_ENTRIES == 16);

        let mut cfg = [PmpCfgVal::default(); NUM_ENTRIES];
        let mut addr = [0usize; NUM_ENTRIES];

        unsafe {
            // TODO: use zerocopy to perform this transmute.
            let config: &mut [usize; 4] = core::mem::transmute(&mut cfg);
            asm!("csrr {cfg}, pmpcfg0", cfg = out(reg) config[0]);
            asm!("csrr {cfg}, pmpcfg1", cfg = out(reg) config[1]);
            asm!("csrr {cfg}, pmpcfg2", cfg = out(reg) config[2]);
            asm!("csrr {cfg}, pmpcfg3", cfg = out(reg) config[3]);

            asm!("csrr {addr}, pmpaddr0", addr = out(reg) addr[0]);
            asm!("csrr {addr}, pmpaddr1", addr = out(reg) addr[1]);
            asm!("csrr {addr}, pmpaddr2", addr = out(reg) addr[2]);
            asm!("csrr {addr}, pmpaddr3", addr = out(reg) addr[3]);
            asm!("csrr {addr}, pmpaddr4", addr = out(reg) addr[4]);
            asm!("csrr {addr}, pmpaddr5", addr = out(reg) addr[5]);
            asm!("csrr {addr}, pmpaddr6", addr = out(reg) addr[6]);
            asm!("csrr {addr}, pmpaddr7", addr = out(reg) addr[7]);
            asm!("csrr {addr}, pmpaddr8", addr = out(reg) addr[8]);
            asm!("csrr {addr}, pmpaddr9", addr = out(reg) addr[9]);
            asm!("csrr {addr}, pmpaddr10", addr = out(reg) addr[10]);
            asm!("csrr {addr}, pmpaddr11", addr = out(reg) addr[11]);
            asm!("csrr {addr}, pmpaddr12", addr = out(reg) addr[12]);
            asm!("csrr {addr}, pmpaddr13", addr = out(reg) addr[13]);
            asm!("csrr {addr}, pmpaddr14", addr = out(reg) addr[14]);
            asm!("csrr {addr}, pmpaddr15", addr = out(reg) addr[15]);
        }
        Self { cfg, addr }
    }

    /// Log the details of the PMP configuration.
    pub fn dump(&self) {
        for (i, (cfg, address)) in self.cfg.iter().zip(self.addr.iter()).enumerate() {
            let mut size = 0usize;
            let mut addr = *address;
            let mode = match cfg.a() {
                PmpCfgAddressMode::Off => "---",
                PmpCfgAddressMode::Tor => {
                    size = (addr - self.addr[i - 1]) << 2;
                    "TOR"
                }
                PmpCfgAddressMode::Na4 => {
                    size = 4;
                    "NA4"
                }
                PmpCfgAddressMode::Napot => {
                    size = 1 << (!addr).trailing_zeros();
                    addr = addr & !(size - 1);
                    size <<= 3;
                    "NPT"
                }
            };
            addr <<= 2;
            pw_log::debug!(
                "{:2}: {:#010x} {} {}{}{}{} sz={:#010x}",
                i as usize,
                addr as usize,
                mode as &str,
                if cfg.l() { 'L' } else { '-' } as char,
                if cfg.x() { 'X' } else { '-' } as char,
                if cfg.w() { 'W' } else { '-' } as char,
                if cfg.r() { 'R' } else { '-' } as char,
                size as usize,
            );
        }
    }
}
