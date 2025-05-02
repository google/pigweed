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

pub struct Scb {
    pub shcsr: Shcsr,
}

impl Scb {
    pub(super) const fn new() -> Self {
        Self { shcsr: Shcsr }
    }
}

pub struct ShcsrVal(u32);
impl ShcsrVal {
    rw_bool_field!(u32, mem_fault_act, 0);
    rw_bool_field!(u32, bus_fault_act, 1);
    rw_bool_field!(u32, hard_fault_act, 2);
    rw_bool_field!(u32, usg_fault_act, 3);
    rw_bool_field!(u32, secure_fault_act, 4);
    rw_bool_field!(u32, nmi_act, 5);
    rw_bool_field!(u32, svcall_act, 7);
    rw_bool_field!(u32, monitor_act, 8);
    rw_bool_field!(u32, pendsv_act, 10);
    rw_bool_field!(u32, systick_act, 11);
    rw_bool_field!(u32, usg_fault_pended, 12);
    rw_bool_field!(u32, mem_fault_pended, 13);
    rw_bool_field!(u32, bus_fault_pended, 14);
    rw_bool_field!(u32, svcall_pended, 15);
    rw_bool_field!(u32, mem_fault_ena, 16);
    rw_bool_field!(u32, bus_fault_ena, 17);
    rw_bool_field!(u32, usg_fault_ena, 18);
    rw_bool_field!(u32, secure_fault_ena, 19);
    rw_bool_field!(u32, secure_fault_pended, 20);
    rw_bool_field!(u32, hard_fault_pended, 21);
}
rw_reg!(Shcsr, ShcsrVal, 0xe000ed24);
