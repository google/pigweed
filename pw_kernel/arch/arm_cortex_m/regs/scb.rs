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

/// System Control Block register bank.
///
/// Note: non-exhaustive list of registers.
pub struct Scb {
    /// System Handler Control and State Register
    pub shcsr: Shcsr,
}

impl Scb {
    pub(super) const fn new() -> Self {
        Self { shcsr: Shcsr }
    }
}

#[repr(transparent)]
pub struct ShcsrVal(u32);
impl ShcsrVal {
    rw_bool_field!(u32, mem_fault_act, 0, "MemManage exception active state");
    rw_bool_field!(u32, bus_fault_act, 1, "BusFault exception active state");
    rw_bool_field!(u32, hard_fault_act, 2, "HardFault exception active state");
    rw_bool_field!(u32, usg_fault_act, 3, "UsageFault exception active state");
    rw_bool_field!(
        u32,
        secure_fault_act,
        4,
        "SecureFault exception active state"
    );
    rw_bool_field!(u32, nmi_act, 5, "NMI exception active state");
    rw_bool_field!(u32, svcall_act, 7, "SVCall exception active state");
    rw_bool_field!(u32, monitor_act, 8, "DebugMonitor exception active state");
    rw_bool_field!(u32, pendsv_act, 10, "PendSV exception active state");
    rw_bool_field!(u32, systick_act, 11, "SysTick exception active state");
    rw_bool_field!(
        u32,
        usg_fault_pended,
        12,
        "UsageFault exception pended state"
    );
    rw_bool_field!(
        u32,
        mem_fault_pended,
        13,
        "MemManage exception pended state"
    );
    rw_bool_field!(u32, bus_fault_pended, 14, "BusFault exception pended state");
    rw_bool_field!(u32, svcall_pended, 15, "SVCall exception pended state");
    rw_bool_field!(u32, mem_fault_ena, 16, "MemManage exception enable");
    rw_bool_field!(u32, bus_fault_ena, 17, "BusFault exception enable");
    rw_bool_field!(u32, usg_fault_ena, 18, "UsageFault exception enable");
    rw_bool_field!(u32, secure_fault_ena, 19, "SecureFault exception enable");
    rw_bool_field!(
        u32,
        secure_fault_pended,
        20,
        "SecureFault exception pended state"
    );
    rw_bool_field!(
        u32,
        hard_fault_pended,
        21,
        "HardFault exception pended state"
    );
}
rw_reg!(
    Shcsr,
    ShcsrVal,
    0xe000ed24,
    "SCB System Handler Control and State Register"
);
