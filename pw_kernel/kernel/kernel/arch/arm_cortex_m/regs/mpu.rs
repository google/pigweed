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

pub struct Mpu {
    pub _type: Type,
    pub ctrl: Ctrl,
    pub rnr: Rnr,
    pub rbar: Rbar,
    pub rlar: Rlar,
    pub mair0: Mair0,
    pub mair1: Mair1,
}

impl Mpu {
    pub(super) const fn new() -> Self {
        Self {
            _type: Type,
            ctrl: Ctrl,
            rnr: Rnr,
            rbar: Rbar,
            rlar: Rlar,
            mair0: Mair0,
            mair1: Mair1,
        }
    }
}

pub struct TypeVal(u32);
impl TypeVal {
    ro_bool_field!(separate, 0);
    ro_int_field!(dregion, 8, 15, u8);
}
ro_reg!(Type, TypeVal, 0xE000ED90);

pub struct CtrlVal(u32);
impl CtrlVal {
    rw_bool_field!(enable, 0);
    rw_bool_field!(hfnmiena, 1);
    rw_bool_field!(privdefena, 2);
}
rw_reg!(Ctrl, CtrlVal, 0xE000ED94);

#[derive(Default)]
pub struct RnrVal(u32);
impl RnrVal {
    rw_int_field!(region, 0, 7, u8);
}
rw_reg!(Rnr, RnrVal, 0xE000ED98);

#[repr(u8)]
pub enum RbarAp {
    RwPrivileged = 0b00,
    RwAny = 0b01,
    RoPrivileged = 0b10,
    RoAny = 0b11,
}

#[repr(u8)]
pub enum RbarSh {
    NonShareable = 0b00,
    Reserved = 0b01,
    OuterShareable = 0b10,
    InnerShareable = 0b11,
}

#[derive(Copy, Clone, Default)]
pub struct RbarVal(u32);
impl RbarVal {
    rw_bool_field!(xn, 0);

    pub const fn ap(&self) -> RbarAp {
        // Safety: Value is masked to only contain valid enum values.
        unsafe { core::mem::transmute(ops::get_u32(self.0, 1, 2) as u8) }
    }

    pub const fn with_ap(self, val: RbarAp) -> Self {
        Self(ops::set_u32(self.0, 1, 2, val as u32))
    }

    pub const fn sh(&self) -> RbarSh {
        // Safety: Value is masked to only contain valid enum values.
        unsafe { core::mem::transmute(ops::get_u32(self.0, 3, 4) as u8) }
    }

    pub const fn with_sh(self, val: RbarSh) -> Self {
        Self(ops::set_u32(self.0, 3, 4, val as u32))
    }

    rw_masked_field!(base, 0xffff_ffe0);
}
rw_reg!(Rbar, RbarVal, 0xE000ED9C);

#[derive(Copy, Clone, Default)]
pub struct RlarVal(u32);
impl RlarVal {
    rw_bool_field!(en, 0);
    rw_int_field!(attrindx, 1, 3, u8);
    rw_bool_field!(pxn, 4);
    rw_masked_field!(limit, 0xffff_ffe0);
}
rw_reg!(Rlar, RlarVal, 0xE000EDA0);

#[repr(u8)]
pub enum MairDeviceMemoryOrdering {
    /// non-Gathering, non-Reordering, no Early Write acknowledgement.
    #[allow(non_camel_case_types)]
    nGnRnE = 0b00,

    /// non-Gathering, non-Reordering, Early Write acknowledgement.
    #[allow(non_camel_case_types)]
    nGnRE = 0b01,

    /// non-Gathering, Reordering, Early Write acknowledgement.
    #[allow(non_camel_case_types)]
    nGRE = 0b10,

    /// Gathering, Reordering, Early Write acknowledgement.
    GRE = 0b11,
}

#[repr(u8)]
pub enum MairNormalMemoryCaching {
    /// Write-Through Transient Write Only
    WriteThroughTransientWO = 0b0001,

    /// Write-Through Transient Read Only
    WriteThroughTransientRO = 0b0010,

    /// Write-Through Transient Read / Write
    WriteThroughTransientRW = 0b0011,

    /// Non-cacheable
    NonCacheable = 0b0100,

    /// Write-Back Transient Write Only
    WriteBackTransientWO = 0b0101,

    /// Write-Back Transient Read Only
    WriteBackTransientRO = 0b0110,

    /// Write-Back Transient Read / Write
    WriteBackTransientRW = 0b0111,

    /// Write-Through Non-Transient Write Only
    WriteThroughNonTransientWO = 0b1001,

    /// Write-Through Non-Transient Read Only
    WriteThroughNonTransientRO = 0b1010,

    /// Write-Through Non-Transient Read / Write
    WriteThroughNonTransientRW = 0b1011,

    /// Write-Back Non-Transient Write Only
    WriteBackNonTransientWO = 0b1101,

    /// Write-Back Non-Transient Read Only
    WriteBackNonTransientRO = 0b1110,

    /// Write-Back Non-Transient Read / Write
    WriteBackNonTransientRW = 0b1111,
}

/// There are notably no accessors for `MairAttr` because it's unclear
/// how they would be used at this time and therefore difficult to build
/// them for optimal code gen.
pub struct MairAttr(u8);

impl MairAttr {
    pub const fn device_memory(ordering: MairDeviceMemoryOrdering) -> Self {
        // Value layout for device memory:
        // | 7     4 | 3     2 | 1  0 |
        // +--------+----------+------+
        // | 0b0000 | ordering | RES0 |
        let ordering = ordering as u8;
        Self(ordering << 2)
    }

    pub const fn normal_memory(
        inner: MairNormalMemoryCaching,
        outer: MairNormalMemoryCaching,
    ) -> Self {
        // Value layout for normal memory:
        // | 7      4 | 3      0 |
        // +----------+----------+
        // |  outer   |  inner   |
        let outer = outer as u8;
        let inner = inner as u8;
        Self((outer << 4) | inner)
    }
}

macro_rules! attr_field {
    ($name:ident, $start:literal, $end:literal) => {
        pub const fn $name(&self) -> u8 {
            ops::get_u32(self.0, $start, $end) as u8
        }
        paste::paste! {
            pub const fn [<with_ $name>](&mut self, val: MairAttr) -> Self {
                Self(ops::set_u32(self.0, $start, $end, val.0 as u32))
            }
        }
    };
}

#[derive(Default)]
pub struct Mair0Val(u32);
impl Mair0Val {
    attr_field!(attr0, 0, 7);
    attr_field!(attr1, 8, 15);
    attr_field!(attr2, 16, 23);
    attr_field!(attr3, 24, 31);
}
rw_reg!(Mair0, Mair0Val, 0xE000EDC0);

pub struct Mair1Val(u32);
impl Mair1Val {
    attr_field!(attr4, 0, 7);
    attr_field!(attr5, 8, 15);
    attr_field!(attr6, 16, 23);
    attr_field!(attr7, 24, 31);
}
rw_reg!(Mair1, Mair1Val, 0xE000EDC4);
