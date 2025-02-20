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

use regs::*;

pub struct SysTick {
    pub csr: Csr,
    pub rvr: Rvr,
    pub cvr: Cvr,
    pub calib: Calib,
}

impl SysTick {
    pub(super) const fn new() -> Self {
        Self {
            csr: Csr,
            rvr: Rvr,
            cvr: Cvr,
            calib: Calib,
        }
    }
}

#[repr(u8)]
pub enum CsrClkSource {
    External = 0b0,
    PE = 0b1,
}

pub struct CsrVal(u32);
impl CsrVal {
    rw_bool_field!(enable, 0);
    rw_bool_field!(tickint, 1);

    pub const fn clksource(&self) -> CsrClkSource {
        // Safety: Value is masked to only contain valid enum values.
        unsafe { core::mem::transmute(ops::get_u32(self.0, 2, 2) as u8) }
    }

    pub const fn with_clksource(self, val: CsrClkSource) -> Self {
        Self(ops::set_u32(self.0, 2, 2, val as u32))
    }

    rw_bool_field!(countflag, 16);
}
rw_reg!(Csr, CsrVal, 0xe000e010);

pub struct RvrVal(u32);
impl RvrVal {
    rw_int_field!(reload, 0, 23, u32);
}
rw_reg!(Rvr, RvrVal, 0xe000e014);

pub struct CvrVal(u32);
impl CvrVal {
    rw_int_field!(current, 0, 23, u32);
}
rw_reg!(Cvr, CvrVal, 0xe000e018);

pub struct CalibVal(u32);
impl CalibVal {
    rw_int_field!(tenms, 0, 23, u32);
    rw_bool_field!(skew, 30);
    rw_bool_field!(noref, 31);
}
rw_reg!(Calib, CalibVal, 0xe000e01c);
