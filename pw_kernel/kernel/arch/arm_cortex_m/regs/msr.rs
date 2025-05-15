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

use regs::*;

#[allow(unused_macros)]
macro_rules! ro_msr_reg {
    ($name:ident, $val_type:ident, $reg_ame:ident, $doc:literal) => {
        #[doc=$doc]
        pub struct $name;
        impl $name {
            #[inline]
            pub fn read() -> $val_type {
                let mut val: usize;
                unsafe {
                    core::arch::asm!(concat!("mrs {0}, ", stringify!($reg_name)), out(reg) val)
                };
                $val_type(val as u32)
            }
        }
    };
}

macro_rules! rw_msr_reg {
    ($name:ident, $val_type:ident, $reg_name:ident, $doc:literal) => {
        ro_msr_reg!($name, $val_type, $reg_name, $doc);
        impl $name {
            #[allow(dead_code)]
            #[inline]
            pub fn write(val: $val_type) {
                unsafe {
                    core::arch::asm!(
                        concat!("msr ", stringify!($reg_name), ", {0}"),
                        in(reg) val.0)
                };
            }
        }
    };
}

/// Stack-pointer selection
#[allow(dead_code)]
#[repr(u32)]
pub enum Spsel {
    Main = 0,
    Process = 1,
}

#[derive(Copy, Clone, Default)]
#[repr(transparent)]
pub struct ControlVal(pub u32);

impl ControlVal {
    rw_bool_field!(u32, npriv, 0, "non privileged");
    rw_enum_field!(u32, spsel, 1, 1, Spsel, "stack-pointer select");
    rw_bool_field!(u32, fpca, 2, "floating-point context active");
    rw_bool_field!(u32, sfpa, 3, "secure floating-point active");
    rw_bool_field!(
        u32,
        bti_en,
        4,
        "privileged branch target identification enable"
    );
    rw_bool_field!(
        u32,
        ubti_en,
        5,
        "un-privileged branch target identification enable"
    );
    rw_bool_field!(u32, pac_en, 6, "privileged pointer authentication enable");
    rw_bool_field!(
        u32,
        upac_en,
        7,
        "un-privileged pointer authentication enable"
    );
}

rw_msr_reg!(Control, ControlVal, control, "Control Register");
