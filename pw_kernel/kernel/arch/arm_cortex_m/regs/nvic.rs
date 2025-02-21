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

const ISER_BASE: *mut u32 = 0xe000e100 as *mut u32;
const ICER_BASE: *mut u32 = 0xe000e180 as *mut u32;
const ISPR_BASE: *mut u32 = 0xe000e200 as *mut u32;
const ICPR_BASE: *mut u32 = 0xe000e280 as *mut u32;
const IABR_BASE: *mut u32 = 0xe000e300 as *mut u32;
const IPR_BASE: *mut u32 = 0xe000e400 as *mut u32;

const unsafe fn bit_reg_and_mask(index: usize, reg_base: *mut u32) -> (*mut u32, u32) {
    assert!(index < 32 * 16);
    let offset = index / 32;
    let mask = 1 << (index % 32);
    (reg_base.add(offset), mask)
}

unsafe fn get_indexed_bit(index: usize, reg_base: *mut u32) -> bool {
    let (reg, mask) = bit_reg_and_mask(index, reg_base);
    (reg.read_volatile() & mask) != 0
}

unsafe fn set_indexed_bit(index: usize, reg_base: *mut u32) {
    let (reg, mask) = bit_reg_and_mask(index, reg_base);
    reg.write_volatile(mask)
}

const unsafe fn priority_reg_and_offset(index: usize, reg_base: *mut u32) -> (*mut u32, usize) {
    assert!(index < 32 * 16);
    let reg_offset = index / 4;
    let field_offset = (index % 4) * 8;
    (reg_base.add(reg_offset), field_offset)
}

pub struct Nvic {}

impl Nvic {
    pub(super) const fn new() -> Self {
        Self {}
    }

    #[inline]
    pub fn is_enabled(&self, index: usize) -> bool {
        unsafe { get_indexed_bit(index, ISER_BASE) }
    }

    #[inline]
    pub fn enable(&mut self, index: usize) {
        unsafe { set_indexed_bit(index, ISER_BASE) }
    }

    #[inline]
    pub fn disable(&mut self, index: usize) {
        unsafe { set_indexed_bit(index, ICER_BASE) }
    }

    #[inline]
    pub fn is_pending(&self, index: usize) -> bool {
        unsafe { get_indexed_bit(index, ISPR_BASE) }
    }

    pub fn set_pending(&mut self, index: usize) {
        unsafe { set_indexed_bit(index, ISPR_BASE) }
    }

    pub fn clear_pending(&mut self, index: usize) {
        unsafe { set_indexed_bit(index, ICPR_BASE) }
    }

    pub fn is_active(&self, index: usize) -> bool {
        unsafe { get_indexed_bit(index, IABR_BASE) }
    }

    pub fn get_active_raw(&self) -> [u32; 16] {
        // If this ends up being too much code space, it could be replaced with
        // a loop filling a MaybeUninit<[u32; 16]>.  Since this is primarily
        // meant as a debug utility, that may not be a necessary optimization.
        unsafe {
            [
                IABR_BASE.add(0).read_volatile(),
                IABR_BASE.add(1).read_volatile(),
                IABR_BASE.add(2).read_volatile(),
                IABR_BASE.add(3).read_volatile(),
                IABR_BASE.add(4).read_volatile(),
                IABR_BASE.add(5).read_volatile(),
                IABR_BASE.add(6).read_volatile(),
                IABR_BASE.add(7).read_volatile(),
                IABR_BASE.add(8).read_volatile(),
                IABR_BASE.add(9).read_volatile(),
                IABR_BASE.add(10).read_volatile(),
                IABR_BASE.add(11).read_volatile(),
                IABR_BASE.add(12).read_volatile(),
                IABR_BASE.add(13).read_volatile(),
                IABR_BASE.add(14).read_volatile(),
                IABR_BASE.add(15).read_volatile(),
            ]
        }
    }

    pub fn get_priority(&self, index: usize) -> u8 {
        unsafe {
            let (reg, offset) = priority_reg_and_offset(index, IPR_BASE);
            let val = reg.read_volatile();
            ((val >> offset) & 0xff) as u8
        }
    }

    pub fn set_priority(&self, index: usize, priority: u8) {
        unsafe {
            let (reg, offset) = priority_reg_and_offset(index, IPR_BASE);
            let val = reg.read_volatile();
            reg.write_volatile((val & (0xff << offset)) | ((priority as u32) << offset))
        }
    }
}
