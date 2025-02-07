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

use cortex_m::peripheral::Peripherals;
use pw_log::info;

// Start of trying to read features out of CPUID and other registers in hardware.

// Not complete and currently unused.

#[derive(PartialEq, PartialOrd)]
pub enum ArchVersion {
    ArmV6m = 0,
    ArmV7m = 1,
    ARMv8m = 2,
}
pub struct Features {
    arch_version: ArchVersion,
    has_vtor: bool,
}

impl Features {
    pub const fn new() -> Features {
        return Features {
            arch_version: ArchVersion::ArmV6m,
            has_vtor: false,
        };
    }

    pub fn read_features(&mut self) {
        let p = Peripherals::take().unwrap();
        let cpuid = p.CPUID.base.read();
        info!("CPUID 0x{:x}", cpuid);

        let mut f = Features::new();
        if (cpuid >> 16 & 0xf) != 0xf {
            // ARMv6m has an architecture field of 0xc
            f.arch_version = ArchVersion::ArmV6m;
            f.has_vtor = true;
        } else {
            // TODO: figure out more arch version bits here
            f.arch_version = ArchVersion::ArmV7m;
            f.has_vtor = true;
        }
    }

    pub fn at_least_version(&self, version: ArchVersion) -> bool {
        if version <= self.arch_version {
            return true;
        }
        return false;
    }
}
