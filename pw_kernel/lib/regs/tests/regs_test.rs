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
#![no_main]

use regs::ops;
use unittest::test;

#[test]
fn mask_calculated_correctly() -> unittest::Result<()> {
    unittest::assert_eq!(ops::mask(8, 15), 0x0000_ff00);
    Ok(())
}

#[test]
fn get_bool_extracts_correct_value() -> unittest::Result<()> {
    unittest::assert_false!(ops::get_bool(0x0000_0100, 7));
    unittest::assert_true!(ops::get_bool(0x0000_0100, 8));
    unittest::assert_false!(ops::get_bool(0x0000_0100, 9));
    Ok(())
}

#[test]
fn set_bool_preserved_unmasked_value() -> unittest::Result<()> {
    unittest::assert_eq!(ops::set_bool(0xffff_ffff, 16, false), 0xfffe_ffff);
    Ok(())
}

#[test]
fn get_u32_extracts_correct_value() -> unittest::Result<()> {
    unittest::assert_eq!(ops::get_u32(0x5555_aa55, 8, 15), 0xaa);
    Ok(())
}

#[test]
fn set_u32_preserves_unmasked_value() -> unittest::Result<()> {
    unittest::assert_eq!(ops::set_u32(0x5555_5555, 8, 15, 0xaa), 0x5555_aa55);
    Ok(())
}

#[test]
fn single_bit_get_u32_extracts_correct_value() -> unittest::Result<()> {
    unittest::assert_eq!(ops::get_u32(0x0001_0000, 16, 16), 0b1);
    Ok(())
}

#[test]
fn single_bit_set_u32_sets_correct_value() -> unittest::Result<()> {
    unittest::assert_eq!(ops::set_u32(0xffff_ffff, 16, 16, 0b0), 0xfffe_ffff);
    Ok(())
}

#[test]
fn get_usize_extracts_correct_value() -> unittest::Result<()> {
    #[cfg(target_pointer_width = "64")]
    unittest::assert_eq!(ops::get_usize(0x5555_aa55_5555_5555, 8 + 32, 15 + 32), 0xaa);
    #[cfg(target_pointer_width = "32")]
    unittest::assert_eq!(ops::get_usize(0x5555_aa55, 8, 15), 0xaa);
    Ok(())
}

#[test]
fn set_usize_preserves_unmasked_value() -> unittest::Result<()> {
    #[cfg(target_pointer_width = "64")]
    unittest::assert_eq!(
        ops::set_usize(0x5555_5555_5555_5555, 8 + 32, 15 + 32, 0xaa),
        0x5555_aa55_5555_5555
    );
    #[cfg(target_pointer_width = "32")]
    unittest::assert_eq!(ops::set_usize(0x5555_5555, 8, 15, 0xaa), 0x5555_aa55);
    Ok(())
}

#[test]
fn single_bit_get_usize_extracts_correct_value() -> unittest::Result<()> {
    #[cfg(target_pointer_width = "64")]
    unittest::assert_eq!(ops::get_usize(0x0001_0000_0000_0000, 48, 48), 0b1);
    #[cfg(target_pointer_width = "32")]
    unittest::assert_eq!(ops::get_usize(0x0001_0000, 16, 16), 0b1);
    Ok(())
}

#[test]
fn single_bit_set_usize_sets_correct_value() -> unittest::Result<()> {
    #[cfg(target_pointer_width = "64")]
    unittest::assert_eq!(
        ops::set_usize(0xffff_ffff_ffff_ffff, 48, 48, 0b0),
        0xfffe_ffff_ffff_ffff
    );
    #[cfg(target_pointer_width = "32")]
    unittest::assert_eq!(ops::set_usize(0xffff_ffff, 16, 16, 0b0), 0xfffe_ffff);
    Ok(())
}
