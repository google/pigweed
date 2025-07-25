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

/// Machine Security Configuration Register value
use crate::rw_csr_reg;
use regs::{ro_bool_field, rw_bool_field};

#[derive(Copy, Clone, Default)]
#[repr(transparent)]
pub struct MSeccfgVal(pub usize);
impl MSeccfgVal {
    rw_bool_field!(usize, mml, 0, "Machine Mode Lockdown");
    // inclusive-language: ignore
    rw_bool_field!(usize, mmwp, 1, "Machine Mode Whitelist Policy");
    rw_bool_field!(usize, rlb, 2, "Rule Locking Bypass");
}

rw_csr_reg!(
    MSeccfg,
    MSeccfgVal,
    mseccfg,
    "Machine Security Configuration Register"
);
