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
macro_rules! ro_csr_reg {
    ($name:ident, $val_type:ident, $reg_ame:ident) => {
        pub struct $name;
        impl $name {
            #[inline]
            pub fn read() -> $val_type {
              mut val: usize;
              core::arch::asm!(concat!("csrr {0}, ", stringify!($reg_name)), out(reg) val);
              $val_type(val)
            }
        }
    };
}

macro_rules! rw_csr_reg {
    ($name:ident, $val_type:ident, $reg_name:ident) => {
        pub struct $name;
        impl $name {
            #[allow(dead_code)]
            #[inline]
            pub fn read() -> $val_type {
              let mut val: usize;
              unsafe {core::arch::asm!(concat!("csrr {0}, ", stringify!($reg_name)), out(reg) val)};
              $val_type(val)
            }

            #[allow(dead_code)]
            #[inline]
            pub fn write(val: $val_type) {
              unsafe {core::arch::asm!(concat!("csrw stringify!($addr), {0}", stringify!($reg_name)), in(reg) val.0)};
            }
        }
    };
}

#[derive(Clone, Copy)]
#[repr(usize)]
#[non_exhaustive]
#[allow(dead_code)]
pub enum Exception {
    InstructionAddressMisaligned = 0,
    InstructionAddressFault = 1,
    IllegalInstruction = 2,
    Breakpoint = 3,
    LoadAddressMisaligned = 4,
    LoadAccessFault = 5,
    StoreAddressMisaligned = 6,
    StoreAccessFault = 7,
    EnvironmentCallFromUMode = 8,
    EnvironmentCallFromSMode = 9,
    EnvironmentCallFromMMode = 11,
    InstructionPageFault = 12,
    LoadPageFault = 13,
    StorePageFault = 15,
    SoftwareCheck = 18,
    HardwareError = 19,
}

#[derive(Clone, Copy)]
#[repr(usize)]
#[non_exhaustive]
#[allow(dead_code)]
pub enum Interrupt {
    SupervisorSoftware = 1,
    MachineSoftware = 3,
    SupervisorTimer = 5,
    MachineTimer = 7,
    SupervisorExternal = 9,
    MachineExternal = 11,
    CounterOverflow = 13,
}

#[derive(Clone, Copy)]
pub enum Cause {
    Exception(Exception),
    Interrupt(Interrupt),
}

#[derive(Copy, Clone, Default)]
#[repr(transparent)]
pub struct MCauseVal(pub usize);
impl MCauseVal {
    rw_masked_field!(raw_cause, usize::MAX >> 1, usize);

    #[inline]
    pub fn cause(&self) -> Cause {
        if self.is_interrupt() {
            // Safety: Interrupt is non-exhaustive
            Cause::Interrupt(unsafe { core::mem::transmute::<usize, Interrupt>(self.raw_cause()) })
        } else {
            // Safety: Interrupt is non-exhaustive
            Cause::Exception(unsafe { core::mem::transmute::<usize, Exception>(self.raw_cause()) })
        }
    }

    #[inline]
    pub fn is_interrupt(&self) -> bool {
        // is_negative() is used to test the high bit to be word size independent.
        (self.0 as isize).is_negative()
    }
}

rw_csr_reg!(MCause, MCauseVal, mcause);
