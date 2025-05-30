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

use super::Instr;
use crate::riscv::Instr32B;
use crate::riscv::Instr32I;
use crate::riscv::Instr32IS;
use crate::riscv::Instr32J;
use crate::riscv::Instr32R;
use crate::riscv::Instr32S;
use crate::riscv::Instr32U;
use crate::riscv::Opcode;
use crate::riscv::Reg;
use crate::riscv::FUNCT10_ADD;
use crate::riscv::FUNCT10_AND;
use crate::riscv::FUNCT10_OR;
use crate::riscv::FUNCT10_SRAI;
use crate::riscv::FUNCT10_SRLI;
use crate::riscv::FUNCT10_SUB;
use crate::riscv::FUNCT10_XOR;
use crate::riscv::FUNCT3_BRANCH_EQ;
use crate::riscv::FUNCT3_BRANCH_NE;
use crate::riscv::FUNCT3_LOAD_W;
use crate::riscv::FUNCT3_OP_ADDI;
use crate::riscv::FUNCT3_OP_ANDI;
use crate::riscv::FUNCT3_OP_SLI;
use crate::riscv::FUNCT3_STORE_W;
const OP_MASK_5: u16 = 0xe003;
const OP_MASK_6: u16 = 0xf003;
const OP_MASK_7: u16 = 0xec03;
const OP_MASK_9: u16 = 0xfc63;
macro_rules! static_assert {
    ($expression:expr) => {
        const _: () = assert!($expression);
    };
}
/// Decodes a 16-bit RV32IMC compressed instruction `instr` into a 32-bit RV32IM instruction.
///
pub fn decompress_instr(instr: u16) -> Option<Instr> {
    match instr & OP_MASK_5 {
        rv16::CJ::OP_CODE => {
            static_assert!(rv16::CJ::OP_MASK == OP_MASK_5);
            let instr = rv16::CJ::from_bits(instr);
            let mut result = Instr32J::from_bits(0);
            result.set_opcode(Opcode::JAL);
            result.set_imm(instr.imm().into());
            result.set_rd(Reg::X0);
            return Some(Instr(result.into_bits()));
        }
        rv16::CJal::OP_CODE => {
            static_assert!(rv16::CJal::OP_MASK == OP_MASK_5);
            let instr = rv16::CJal::from_bits(instr);
            let mut result = Instr32J(0);
            result.set_opcode(Opcode::JAL);
            result.set_imm(i32::from(instr.imm()));
            result.set_rd(Reg::X1Ra);
            return Some(Instr(result.into_bits()));
        }
        rv16::CLw::OP_CODE => {
            static_assert!(rv16::CLw::OP_MASK == OP_MASK_5);
            let instr = rv16::CLw::from_bits(instr);
            let mut result = Instr32I::from_bits(0);
            result.set_opcode(Opcode::LOAD);
            result.set_funct3(FUNCT3_LOAD_W);
            result.set_uimm(instr.uimm().into());
            result.set_rs1(instr.rs1());
            result.set_rd(instr.rd());
            return Some(Instr(result.into_bits()));
        }
        rv16::CSw::OP_CODE => {
            static_assert!(rv16::CSw::OP_MASK == OP_MASK_5);
            let instr = rv16::CSw::from_bits(instr);
            let mut result = Instr32S::from_bits(0);
            result.set_opcode(Opcode::STORE);
            result.set_funct3(FUNCT3_STORE_W);
            result.set_uimm(instr.uimm().into());
            result.set_rs1(instr.rs1());
            result.set_rs2(instr.rs2());
            return Some(Instr(result.into_bits()));
        }
        rv16::CLwsp::OP_CODE => {
            static_assert!(rv16::CLwsp::OP_MASK == OP_MASK_5);
            let instr = rv16::CLwsp::from_bits(instr);
            if instr.rd() != Reg::X0 {
                let mut result = Instr32I::from_bits(0);
                result.set_opcode(Opcode::LOAD);
                result.set_funct3(FUNCT3_LOAD_W);
                result.set_uimm(instr.uimm().into());
                result.set_rs1(Reg::X2Sp);
                result.set_rd(instr.rd());
                return Some(Instr(result.into_bits()));
            }
        }
        rv16::CSwsp::OP_CODE => {
            static_assert!(rv16::CSwsp::OP_MASK == OP_MASK_5);
            let instr = rv16::CSwsp::from_bits(instr);
            let mut result = Instr32S::from_bits(0);
            result.set_opcode(Opcode::STORE);
            result.set_funct3(FUNCT3_STORE_W);
            result.set_uimm(instr.uimm().into());
            result.set_rs1(Reg::X2Sp);
            result.set_rs2(instr.rs2());
            return Some(Instr(result.into_bits()));
        }
        rv16::CAddi::OP_CODE => {
            static_assert!(rv16::CAddi::OP_MASK == OP_MASK_5);
            let instr = rv16::CAddi::from_bits(instr);
            let mut result = Instr32I::from_bits(0);
            result.set_opcode(Opcode::OP_IMM);
            result.set_funct3(FUNCT3_OP_ADDI);
            result.set_imm(instr.nzimm().into());
            result.set_rs1(instr.rs1rd());
            result.set_rd(instr.rs1rd());
            return Some(Instr(result.into_bits()));
        }
        rv16::CAddi4spn::OP_CODE => {
            static_assert!(rv16::CAddi4spn::OP_MASK == OP_MASK_5);
            let instr = rv16::CAddi4spn::from_bits(instr);
            if instr.nzuimm() != 0 {
                let mut result = Instr32I::from_bits(0);
                result.set_opcode(Opcode::OP_IMM);
                result.set_funct3(FUNCT3_OP_ADDI);
                result.set_imm(instr.nzuimm().into());
                result.set_rs1(Reg::X2Sp);
                result.set_rd(instr.rd());
                return Some(Instr(result.into_bits()));
            }
        }
        rv16::CLi::OP_CODE => {
            static_assert!(rv16::CLui::OP_MASK == OP_MASK_5);
            let instr = rv16::CLi::from_bits(instr);
            let mut result = Instr32I::from_bits(0);
            result.set_opcode(Opcode::OP_IMM);
            result.set_funct3(FUNCT3_OP_ADDI);
            result.set_imm(instr.nzimm());
            result.set_rs1(Reg::X0);
            result.set_rd(instr.rd());
            return Some(Instr(result.into_bits()));
        }
        rv16::CLui::OP_CODE => {
            static_assert!(rv16::CLui::OP_MASK == OP_MASK_5);
            let instr = rv16::CLui::from_bits(instr);
            if instr.nzimm() != 0 {
                if instr.rd() == Reg::X2Sp {
                    static_assert!(rv16::CAddi16sp::OP_MASK == OP_MASK_5);
                    static_assert!(rv16::CAddi16sp::OP_CODE == rv16::CLui::OP_CODE);
                    let instr = rv16::CAddi16sp::from_bits(instr.into_bits());
                    let mut result = Instr32I::from_bits(0);
                    result.set_opcode(Opcode::OP_IMM);
                    result.set_funct3(FUNCT3_OP_ADDI);
                    result.set_imm(instr.nzimm());
                    result.set_rs1(Reg::X2Sp);
                    result.set_rd(Reg::X2Sp);
                    return Some(Instr(result.into_bits()));
                } else {
                    let mut result = Instr32U::from_bits(0);
                    result.set_opcode(Opcode::LUI);
                    result.set_imm(instr.nzimm() >> 12);
                    result.set_rd(instr.rd());
                    return Some(Instr(result.into_bits()));
                }
            }
        }
        rv16::CBeqz::OP_CODE => {
            static_assert!(rv16::CBeqz::OP_MASK == OP_MASK_5);
            let instr = rv16::CBeqz::from_bits(instr);
            let mut result = Instr32B::from_bits(0);
            result.set_opcode(Opcode::BRANCH);
            result.set_funct3(FUNCT3_BRANCH_EQ);
            result.set_imm(i32::from(instr.offset()));
            result.set_rs1(instr.rs1());
            result.set_rs2(Reg::X0);
            return Some(Instr(result.into_bits()));
        }
        rv16::CBnez::OP_CODE => {
            static_assert!(rv16::CBnez::OP_MASK == OP_MASK_5);
            let instr = rv16::CBnez::from_bits(instr);
            let mut result = Instr32B::from_bits(0);
            result.set_opcode(Opcode::BRANCH);
            result.set_funct3(FUNCT3_BRANCH_NE);
            result.set_imm(i32::from(instr.offset()));
            result.set_rs1(instr.rs1());
            result.set_rs2(Reg::X0);
            return Some(Instr(result.into_bits()));
        }
        rv16::CSlli::OP_CODE => {
            static_assert!(rv16::CSlli::OP_MASK == OP_MASK_5);
            let instr = rv16::CSlli::from_bits(instr);
            let mut result = Instr32I::from_bits(0);
            // For RV32C, code points with shamt[5] == 1 are reserved
            if instr.shamt() & 0x20 == 0 {
                result.set_opcode(Opcode::OP_IMM);
                result.set_funct3(FUNCT3_OP_SLI);
                result.set_uimm(instr.shamt().into());
                result.set_rs1(instr.rs1rd());
                result.set_rd(instr.rs1rd());
                return Some(Instr(result.into_bits()));
            }
        }
        _ => {}
    }
    match instr & OP_MASK_6 {
        rv16::CMv::OP_CODE => {
            static_assert!(rv16::CMv::OP_MASK == OP_MASK_6);
            let instr = rv16::CMv::from_bits(instr);
            if instr.rs2() == Reg::X0 {
                let instr = rv16::CJr::from_bits(instr.into_bits());
                let mut result = Instr32I::from_bits(0);
                result.set_opcode(Opcode::JALR);
                result.set_funct3(0);
                result.set_imm(0);
                result.set_rs1(instr.rs1());
                result.set_rd(Reg::X0);
                return Some(Instr(result.into_bits()));
            } else {
                let mut result = Instr32R::from_bits(0);
                result.set_opcode(Opcode::OP);
                result.set_funct10(FUNCT10_ADD);
                result.set_rs1(Reg::X0);
                result.set_rs2(instr.rs2());
                result.set_rd(instr.rd());
                return Some(Instr(result.into_bits()));
            }
        }
        rv16::CAdd::OP_CODE => {
            static_assert!(rv16::CAdd::OP_MASK == OP_MASK_6);
            let instr = rv16::CAdd::from_bits(instr);
            if instr.rs1rd() == Reg::X0 && instr.rs2() == Reg::X0 {
                // EBREAK
                return Some(Instr(0x100073));
            }
            if instr.rs2() == Reg::X0 {
                let instr = rv16::CJalr::from_bits(instr.into_bits());
                let mut result = Instr32I::from_bits(0);
                result.set_opcode(Opcode::JALR);
                result.set_funct3(0);
                result.set_imm(0);
                result.set_rs1(instr.rs1());
                result.set_rd(Reg::X1Ra);
                return Some(Instr(result.into_bits()));
            } else {
                let mut result = Instr32R::from_bits(0);
                result.set_opcode(Opcode::OP);
                result.set_funct10(FUNCT10_ADD);
                result.set_rs1(instr.rs1rd());
                result.set_rs2(instr.rs2());
                result.set_rd(instr.rs1rd());
                return Some(Instr(result.into_bits()));
            }
        }
        _ => {}
    }
    match instr & OP_MASK_7 {
        rv16::CSrli::OP_CODE => {
            static_assert!(rv16::CSrli::OP_MASK == OP_MASK_7);
            let instr = rv16::CSrli::from_bits(instr);
            // For RV32C, code points with shamt[5] == 1 are reserved
            if instr.shamt() & 0x20 == 0 {
                let mut result = Instr32IS::from_bits(0);
                result.set_opcode(Opcode::OP_IMM);
                result.set_funct10(FUNCT10_SRLI);
                result.set_uimm(instr.shamt());
                result.set_rs1(instr.rs1rd());
                result.set_rd(instr.rs1rd());
                return Some(Instr(result.into_bits()));
            }
        }
        rv16::CSrai::OP_CODE => {
            static_assert!(rv16::CSrai::OP_MASK == OP_MASK_7);
            let instr = rv16::CSrai::from_bits(instr);
            // For RV32C, code points with shamt[5] == 1 are reserved
            if instr.shamt() & 0x20 == 0 {
                let mut result = Instr32IS::from_bits(0);
                result.set_opcode(Opcode::OP_IMM);
                result.set_funct10(FUNCT10_SRAI);
                result.set_uimm(instr.shamt());
                result.set_rs1(instr.rs1rd());
                result.set_rd(instr.rs1rd());
                return Some(Instr(result.into_bits()));
            }
        }
        rv16::CAndi::OP_CODE => {
            static_assert!(rv16::CAndi::OP_MASK == OP_MASK_7);
            let instr = rv16::CAndi::from_bits(instr);
            let mut result = Instr32I::from_bits(0);
            result.set_opcode(Opcode::OP_IMM);
            result.set_funct3(FUNCT3_OP_ANDI);
            result.set_imm(instr.imm().into());
            result.set_rs1(instr.rs1rd());
            result.set_rd(instr.rs1rd());
            return Some(Instr(result.into_bits()));
        }
        _ => {}
    }
    match instr & OP_MASK_9 {
        rv16::CAnd::OP_CODE => {
            static_assert!(rv16::CAnd::OP_MASK == OP_MASK_9);
            let instr = rv16::CAnd::from_bits(instr);
            let mut result = Instr32R::from_bits(0);
            result.set_opcode(Opcode::OP);
            result.set_funct10(FUNCT10_AND);
            result.set_rs1(instr.rs1rd());
            result.set_rs2(instr.rs2());
            result.set_rd(instr.rs1rd());
            return Some(Instr(result.into_bits()));
        }
        rv16::COr::OP_CODE => {
            static_assert!(rv16::COr::OP_MASK == OP_MASK_9);
            let instr = rv16::COr::from_bits(instr);
            let mut result = Instr32R::from_bits(0);
            result.set_opcode(Opcode::OP);
            result.set_funct10(FUNCT10_OR);
            result.set_rs1(instr.rs1rd());
            result.set_rs2(instr.rs2());
            result.set_rd(instr.rs1rd());
            return Some(Instr(result.into_bits()));
        }
        rv16::CXor::OP_CODE => {
            static_assert!(rv16::CXor::OP_MASK == OP_MASK_9);
            let instr = rv16::CXor::from_bits(instr);
            let mut result = Instr32R::from_bits(0);
            result.set_opcode(Opcode::OP);
            result.set_funct10(FUNCT10_XOR);
            result.set_rs1(instr.rs1rd());
            result.set_rs2(instr.rs2());
            result.set_rd(instr.rs1rd());
            return Some(Instr(result.into_bits()));
        }
        rv16::CSub::OP_CODE => {
            static_assert!(rv16::CSub::OP_MASK == OP_MASK_9);
            let instr = rv16::CSub::from_bits(instr);
            let mut result = Instr32R::from_bits(0);
            result.set_opcode(Opcode::OP);
            result.set_funct10(FUNCT10_SUB);
            result.set_rs1(instr.rs1rd());
            result.set_rs2(instr.rs2());
            result.set_rd(instr.rs1rd());
            return Some(Instr(result.into_bits()));
        }
        _ => {}
    }
    None
}
mod rv16 {
    use crate::riscv::{sign_extend_i16, sign_extend_i32, Reg};
    use bitfield_struct::bitfield;
    use pw_cast::try_cast;
    const fn unwrap_or_0(val: Option<u16>) -> u16 {
        match val {
            Some(val) => val,
            None => 0,
        }
    }
    const fn unwrap_or_0_32(val: Option<u32>) -> u32 {
        match val {
            Some(val) => val,
            None => 0,
        }
    }
    const fn bit_range(val: u16, msb: u32, lsb: u32) -> u16 {
        assert!(msb < 16 && lsb < 16);
        (val >> lsb) & (unwrap_or_0(1_u16.checked_shl(msb + 1 - lsb)) - 1)
    }
    const fn set_bit_range(result: &mut u16, msb: u32, lsb: u32, val: u16) {
        assert!(msb < 16 && lsb < 16);
        let mask = unwrap_or_0(1_u16.checked_shl(msb + 1 - lsb)) - 1;
        let val = val & mask;
        *result = (*result & !(mask << lsb)) | (val << lsb);
    }
    const fn set_bit_range_32(result: &mut u32, msb: u32, lsb: u32, val: u32) {
        assert!(msb < 32 && lsb < 32);
        let mask = unwrap_or_0_32(1_u32.checked_shl(msb + 1 - lsb)) - 1;
        let val = val & mask;
        *result = (*result & !(mask << lsb)) | (val << lsb);
    }
    /// RISCV C.LWSP instruction
    #[bitfield(u16)]
    pub struct CLwsp {
        #[bits(7)]
        __: (),
        #[bits(5)]
        pub rd: Reg,
        #[bits(4)]
        __: (),
    }
    impl CLwsp {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x4002;
        pub fn uimm(&self) -> u16 {
            let mut result = 0;
            set_bit_range(&mut result, 5, 5, bit_range(self.0, 12, 12));
            set_bit_range(&mut result, 4, 2, bit_range(self.0, 6, 4));
            set_bit_range(&mut result, 7, 6, bit_range(self.0, 3, 2));
            result
        }
    }
    /// RISCV C.SWSP instruction
    #[bitfield(u16)]
    pub struct CSwsp {
        #[bits(2)]
        __: (),
        #[bits(5)]
        pub rs2: Reg,
        #[bits(9)]
        __: (),
    }
    impl CSwsp {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0xc002;
        pub fn uimm(&self) -> u16 {
            let mut result = 0;
            set_bit_range(&mut result, 5, 2, bit_range(self.0, 12, 9));
            set_bit_range(&mut result, 7, 6, bit_range(self.0, 8, 7));
            result
        }
    }
    /// RISCV C.LW instruction
    #[bitfield(u16)]
    pub struct CLw {
        #[bits(16)]
        __: (),
    }
    impl CLw {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x4000;
        pub const fn rs1(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 9, 7) + 8 => u8).unwrap())
        }
        pub const fn rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 4, 2) + 8 => u8).unwrap())
        }
        pub const fn uimm(&self) -> u16 {
            let mut result = 0;
            set_bit_range(&mut result, 5, 3, bit_range(self.0, 12, 10));
            set_bit_range(&mut result, 2, 2, bit_range(self.0, 6, 6));
            set_bit_range(&mut result, 6, 6, bit_range(self.0, 5, 5));
            result
        }
    }
    /// RISCV C.SW instruction
    #[bitfield(u16)]
    pub struct CSw {
        #[bits(16)]
        __: (),
    }
    impl CSw {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0xc000;
        pub const fn rs1(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 9, 7) + 8 => u8).unwrap())
        }
        pub const fn rs2(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 4, 2) + 8 => u8).unwrap())
        }
        pub const fn uimm(&self) -> u16 {
            let mut result = 0;
            set_bit_range(&mut result, 5, 3, bit_range(self.0, 12, 10));
            set_bit_range(&mut result, 2, 2, bit_range(self.0, 6, 6));
            set_bit_range(&mut result, 6, 6, bit_range(self.0, 5, 5));
            result
        }
    }
    /// RISCV C.J instruction
    #[bitfield(u16)]
    pub struct CJ {
        #[bits(16)]
        __: (),
    }
    impl CJ {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0xa001;
        pub const fn imm(&self) -> i16 {
            let mut result = 0u16;
            set_bit_range(&mut result, 11, 11, bit_range(self.0, 12, 12));
            set_bit_range(&mut result, 4, 4, bit_range(self.0, 11, 11));
            set_bit_range(&mut result, 9, 8, bit_range(self.0, 10, 9));
            set_bit_range(&mut result, 10, 10, bit_range(self.0, 8, 8));
            set_bit_range(&mut result, 6, 6, bit_range(self.0, 7, 7));
            set_bit_range(&mut result, 7, 7, bit_range(self.0, 6, 6));
            set_bit_range(&mut result, 3, 1, bit_range(self.0, 5, 3));
            set_bit_range(&mut result, 5, 5, bit_range(self.0, 2, 2));
            sign_extend_i16(result, 11)
        }
    }
    /// RISCV C.JAL instruction
    #[bitfield(u16)]
    pub struct CJal {
        #[bits(16)]
        __: (),
    }
    impl CJal {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x2001;
        pub const fn imm(&self) -> i16 {
            CJ(self.0).imm()
        }
    }
    /// RISCV C.ADDI instruction
    #[bitfield(u16)]
    pub struct CAddi {
        #[bits(16)]
        __: (),
    }
    impl CAddi {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x0001;
        pub const fn nzimm(&self) -> i16 {
            let mut result = 0u16;
            set_bit_range(&mut result, 5, 5, bit_range(self.0, 12, 12));
            set_bit_range(&mut result, 4, 0, bit_range(self.0, 6, 2));
            sign_extend_i16(result, 5)
        }
        pub const fn rs1rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 11, 7) => u8).unwrap())
        }
    }
    /// RISCV C.ADDI4SPN instruction
    #[bitfield(u16)]
    pub struct CAddi4spn {
        #[bits(16)]
        __: (),
    }
    impl CAddi4spn {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x0000;
        pub const fn nzuimm(&self) -> u16 {
            let mut result = 0u16;
            set_bit_range(&mut result, 5, 4, bit_range(self.0, 12, 11));
            set_bit_range(&mut result, 9, 6, bit_range(self.0, 10, 7));
            set_bit_range(&mut result, 2, 2, bit_range(self.0, 6, 6));
            set_bit_range(&mut result, 3, 3, bit_range(self.0, 5, 5));
            result
        }
        pub fn rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 4, 2) => u8).unwrap() + 8)
        }
    }
    /// RISCV C.ADDI4SPN instruction
    #[bitfield(u16)]
    pub struct CAddi16sp {
        #[bits(16)]
        __: (),
    }
    impl CAddi16sp {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        #[allow(dead_code)]
        pub const OP_CODE: u16 = 0x6001;
        pub fn nzimm(&self) -> i32 {
            let mut result = 0_u32;
            set_bit_range_32(&mut result, 9, 9, u32::from(bit_range(self.0, 12, 12)));
            set_bit_range_32(&mut result, 4, 4, u32::from(bit_range(self.0, 6, 6)));
            set_bit_range_32(&mut result, 6, 6, u32::from(bit_range(self.0, 5, 5)));
            set_bit_range_32(&mut result, 8, 7, u32::from(bit_range(self.0, 4, 3)));
            set_bit_range_32(&mut result, 5, 5, u32::from(bit_range(self.0, 2, 2)));
            sign_extend_i32(result, 9)
        }
    }
    /// RISCV C.LI instruction
    #[bitfield(u16)]
    pub struct CLi {
        #[bits(16)]
        __: (),
    }
    impl CLi {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x4001;
        pub const fn nzimm(&self) -> i32 {
            let mut result = 0u32;
            set_bit_range_32(&mut result, 5, 5, bit_range(self.0, 12, 12) as u32);
            set_bit_range_32(&mut result, 4, 0, bit_range(self.0, 6, 2) as u32);
            sign_extend_i32(result, 5)
        }
        pub const fn rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 11, 7) => u8).unwrap())
        }
    }
    /// RISCV C.LUI instruction
    #[bitfield(u16)]
    pub struct CLui {
        #[bits(16)]
        __: (),
    }
    impl CLui {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x6001;
        pub const fn nzimm(&self) -> i32 {
            let mut d = 0u32;
            set_bit_range_32(&mut d, 5, 5, bit_range(self.0, 12, 12) as u32);
            set_bit_range_32(&mut d, 4, 0, bit_range(self.0, 6, 2) as u32);
            let mut result = 0u32;
            set_bit_range_32(&mut result, 17, 17, bit_range(self.0, 12, 12) as u32);
            set_bit_range_32(&mut result, 16, 12, bit_range(self.0, 6, 2) as u32);
            sign_extend_i32(result, 17)
        }
        pub const fn rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 11, 7) => u8).unwrap())
        }
    }
    /// RISCV C.BEQZ instruction
    #[bitfield(u16)]
    pub struct CBeqz {
        #[bits(16)]
        __: (),
    }
    impl CBeqz {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0xc001;
        pub const fn offset(&self) -> i16 {
            let mut result = 0u16;
            set_bit_range(&mut result, 8, 8, bit_range(self.0, 12, 12));
            set_bit_range(&mut result, 4, 3, bit_range(self.0, 11, 10));
            set_bit_range(&mut result, 7, 6, bit_range(self.0, 6, 5));
            set_bit_range(&mut result, 2, 1, bit_range(self.0, 4, 3));
            set_bit_range(&mut result, 5, 5, bit_range(self.0, 2, 2));
            sign_extend_i16(result, 8)
        }
        pub const fn rs1(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 9, 7) => u8).unwrap() + 8)
        }
    }
    /// RISCV C.BNEZ instruction
    #[bitfield(u16)]
    pub struct CBnez {
        #[bits(16)]
        __: (),
    }
    impl CBnez {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0xe001;
        pub const fn offset(&self) -> i16 {
            CBeqz(self.0).offset()
        }
        pub const fn rs1(&self) -> Reg {
            CBeqz(self.0).rs1()
        }
    }
    /// RISCV C.BNEZ instruction
    #[bitfield(u16)]
    pub struct CSlli {
        #[bits(7)]
        __: (),
        #[bits(5)]
        pub rs1rd: Reg,
        #[bits(4)]
        __: (),
    }
    impl CSlli {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xe003;
        pub const OP_CODE: u16 = 0x0002;
        pub const fn shamt(&self) -> u16 {
            let mut result = 0u16;
            set_bit_range(&mut result, 5, 5, bit_range(self.0, 12, 12));
            set_bit_range(&mut result, 4, 0, bit_range(self.0, 6, 2));
            result
        }
    }
    /// RISCV C.MV instruction
    #[bitfield(u16)]
    pub struct CMv {
        #[bits(2)]
        __: (),
        #[bits(5)]
        pub rs2: Reg,
        #[bits(5)]
        pub rd: Reg,
        #[bits(4)]
        __: (),
    }
    impl CMv {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xf003;
        pub const OP_CODE: u16 = 0x8002;
    }
    /// RISCV C.ADD instruction
    #[bitfield(u16)]
    pub struct CAdd {
        #[bits(2)]
        __: (),
        #[bits(5)]
        pub rs2: Reg,
        #[bits(5)]
        pub rs1rd: Reg,
        #[bits(4)]
        __: (),
    }
    impl CAdd {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xf003;
        pub const OP_CODE: u16 = 0x9002;
    }
    /// RISCV C.JALR instruction
    #[bitfield(u16)]
    pub struct CJalr {
        #[bits(2)]
        __: (),
        #[bits(5)]
        pub rs2: Reg,
        #[bits(5)]
        pub rs1: Reg,
        #[bits(4)]
        __: (),
    }
    impl CJalr {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xf07f;
        #[allow(dead_code)]
        pub const OP_CODE: u16 = 0x9002;
    }
    /// RISCV C.JR instruction
    #[bitfield(u16)]
    pub struct CJr {
        #[bits(2)]
        __: (),
        #[bits(5)]
        pub rs2: Reg,
        #[bits(5)]
        pub rs1: Reg,
        #[bits(4)]
        __: (),
    }
    impl CJr {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xf07f;
        #[allow(dead_code)]
        pub const OP_CODE: u16 = 0x8002;
    }
    /// RISCV C.SRLI instruction
    #[bitfield(u16)]
    pub struct CSrli {
        #[bits(16)]
        __: (),
    }
    impl CSrli {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xec03;
        pub const OP_CODE: u16 = 0x8001;
        pub const fn shamt(&self) -> u32 {
            let mut result = 0;
            set_bit_range_32(&mut result, 5, 5, bit_range(self.0, 12, 12) as u32);
            set_bit_range_32(&mut result, 4, 0, bit_range(self.0, 6, 2) as u32);
            result
        }
        pub const fn rs1rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 9, 7) => u8).unwrap() + 8)
        }
    }
    /// RISCV C.SRAI instruction
    #[bitfield(u16)]
    pub struct CSrai {
        #[bits(16)]
        __: (),
    }
    impl CSrai {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xec03;
        pub const OP_CODE: u16 = 0x8401;
        pub const fn shamt(&self) -> u32 {
            CSrli(self.0).shamt()
        }
        pub const fn rs1rd(&self) -> Reg {
            CSrli(self.0).rs1rd()
        }
    }
    /// RISCV C.ANDI instruction
    #[bitfield(u16)]
    pub struct CAndi {
        #[bits(16)]
        __: (),
    }
    impl CAndi {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xec03;
        pub const OP_CODE: u16 = 0x8801;
        pub const fn imm(&self) -> i16 {
            let mut result = 0;
            set_bit_range(&mut result, 5, 5, bit_range(self.0, 12, 12));
            set_bit_range(&mut result, 4, 0, bit_range(self.0, 6, 2));
            sign_extend_i16(result, 5)
        }
        pub const fn rs1rd(&self) -> Reg {
            CSrli(self.0).rs1rd()
        }
    }
    /// RISCV C.AND instruction
    #[bitfield(u16)]
    pub struct CAnd {
        #[bits(16)]
        __: (),
    }
    impl CAnd {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xfc63;
        pub const OP_CODE: u16 = 0x8c61;
        pub const fn rs1rd(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 9, 7) => u8).unwrap() + 8)
        }
        pub const fn rs2(&self) -> Reg {
            Reg::from_bits(try_cast!(bit_range(self.0, 4, 2) => u8).unwrap() + 8)
        }
    }
    #[bitfield(u16)]
    /// RISCV C.OR instruction
    pub struct COr {
        #[bits(16)]
        __: (),
    }
    impl COr {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xfc63;
        pub const OP_CODE: u16 = 0x8c41;
        pub const fn rs1rd(&self) -> Reg {
            CAnd(self.0).rs1rd()
        }
        pub const fn rs2(&self) -> Reg {
            CAnd(self.0).rs2()
        }
    }
    /// RISCV C.XOR instruction
    #[bitfield(u16)]
    pub struct CXor {
        #[bits(16)]
        __: (),
    }
    impl CXor {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xfc63;
        pub const OP_CODE: u16 = 0x8c21;
        pub const fn rs1rd(&self) -> Reg {
            CAnd(self.0).rs1rd()
        }
        pub const fn rs2(&self) -> Reg {
            CAnd(self.0).rs2()
        }
    }
    /// RISCV C.SUB instruction
    #[bitfield(u16)]
    pub struct CSub {
        #[bits(16)]
        __: (),
    }
    impl CSub {
        #[allow(dead_code)]
        pub const OP_MASK: u16 = 0xfc63;
        pub const OP_CODE: u16 = 0x8c01;
        pub const fn rs1rd(&self) -> Reg {
            CAnd(self.0).rs1rd()
        }
        pub const fn rs2(&self) -> Reg {
            CAnd(self.0).rs2()
        }
    }
    #[cfg(test)]
    mod test {
        use super::bit_range;
        use crate::riscv::compression::rv16::set_bit_range;
        #[test]
        fn test_bit_range() {
            assert_eq!(bit_range(0x56fa, 3, 0), 0xa);
            assert_eq!(bit_range(0x56fa, 7, 4), 0xf);
            assert_eq!(bit_range(0x56fa, 11, 8), 0x6);
            assert_eq!(bit_range(0x56fa, 15, 12), 0x5);
            assert_eq!(bit_range(0x56fa, 2, 1), 0x1);
            assert_eq!(bit_range(0x56fa, 3, 2), 0x2);
            assert_eq!(bit_range(0x56fa, 15, 0), 0x56fa);
        }
        #[test]
        fn test_set_bit_range() {
            let mut result = 0_u16;
            set_bit_range(&mut result, 2, 0, 0xf);
            assert_eq!(result, 0x7);
            set_bit_range(&mut result, 1, 0, 0x0);
            assert_eq!(result, 0x4);
            set_bit_range(&mut result, 3, 1, 0x3);
            assert_eq!(result, 0x6);
            set_bit_range(&mut result, 12, 4, 0x5ba);
            assert_eq!(result, 0x1ba6);
            set_bit_range(&mut result, 15, 0, 0xba5e);
            assert_eq!(result, 0xba5e);
            set_bit_range(&mut result, 15, 0, 0);
            assert_eq!(result, 0x0);
            set_bit_range(&mut result, 15, 0, 0xffff);
            assert_eq!(result, 0xffff);
        }
    }
}
