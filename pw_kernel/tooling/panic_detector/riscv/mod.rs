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

//! Code for parsing and formatting RISC-V instructions.
//
//! Based on the RISC-V specification. See
//! https://github.com/riscv/riscv-isa-manual/ for more information.
pub mod call_graph;
mod compression;
mod elf_mem;
use bitfield_struct::bitfield;
use core::fmt::{Debug, Display};
pub use elf_mem::ElfMem;
/// Risc-V instruction with address
#[derive(Clone, Copy)]
pub struct InstrA {
    pub addr: u32,
    pub instr: Instr,
}
impl InstrA {
    pub fn new(addr: u32, instr: Instr) -> Self {
        Self { addr, instr }
    }
    /// Returns true if this instruction is a 16-bit compressed instruction.
    pub fn is_compressed(&self) -> bool {
        self.instr.is_compressed()
    }
    /// Decodes the instruction
    pub fn decode(&self) -> DecodedInstr {
        self.instr.decode()
    }
    /// If the instruction contains a relative address, this function will
    /// return the absolute address (relative to self.addr).
    pub fn absolute_imm(&self) -> u32 {
        match self.instr.decode().ty() {
            InstrType::R(_) => self.addr,
            InstrType::I(i) => self.addr.wrapping_add_signed(i.imm()),
            InstrType::IS(i) => self.addr.wrapping_add_signed(i.imm()),
            InstrType::IB(_) => self.addr,
            InstrType::S(i) => self.addr.wrapping_add_signed(i.imm()),
            InstrType::B(i) => self.addr.wrapping_add_signed(i.imm()),
            InstrType::U(i) => self.addr.wrapping_add_signed(i.imm()),
            InstrType::J(i) => self.addr.wrapping_add_signed(i.imm()),
            InstrType::Const(_) => self.addr,
            InstrType::Unknown(_) => self.addr,
        }
    }
}
impl Display for InstrA {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{:08x} {}", self.addr, self.instr.decode())
    }
}
impl Debug for InstrA {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "InstrA{{addr: 0x{:08x}, \"{}\"}}",
            self.addr,
            self.instr.decode()
        )
    }
}
/// A raw risc-v instruction. If this instruction is 16-bit, the upper half-word
/// is ignored.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(transparent)]
pub struct Instr(u32);
impl Instr {
    /// Decodes the instruction
    #[inline(always)]
    pub fn decode(self) -> DecodedInstr {
        if self.is_compressed() {
            // The instr is stored in the lower 16-bits.
            let instr: u16 = (self.0 & 0xffff) as u16;
            match compression::decompress_instr(instr) {
                Some(instr) => instr.decode32(),
                None => DecodedInstr::Unknown(instr.into()),
            }
        } else {
            self.decode32()
        }
    }
    #[inline(always)]
    fn decode32(self) -> DecodedInstr {
        match Op7::from_instr(self) {
            Op7::LUI => return DecodedInstr::Lui(self.0.into()),
            Op7::AUIPC => return DecodedInstr::Auipc(self.0.into()),
            Op7::JAL => return DecodedInstr::Jal(self.0.into()),
            _ => {}
        }
        match Op10::from_instr(self) {
            Op10::JALR => return DecodedInstr::Jalr(self.0.into()),
            Op10::BEQ => return DecodedInstr::Beq(self.0.into()),
            Op10::BNE => return DecodedInstr::Bne(self.0.into()),
            Op10::BLT => return DecodedInstr::Blt(self.0.into()),
            Op10::BGE => return DecodedInstr::Bge(self.0.into()),
            Op10::BLTU => return DecodedInstr::Bltu(self.0.into()),
            Op10::BGEU => return DecodedInstr::Bgeu(self.0.into()),
            Op10::LOAD_B => return DecodedInstr::Lb(self.0.into()),
            Op10::LOAD_H => return DecodedInstr::Lh(self.0.into()),
            Op10::LOAD_W => return DecodedInstr::Lw(self.0.into()),
            Op10::LOAD_BU => return DecodedInstr::Lbu(self.0.into()),
            Op10::LOAD_HU => return DecodedInstr::Lhu(self.0.into()),
            Op10::STORE_B => return DecodedInstr::Sb(self.0.into()),
            Op10::STORE_H => return DecodedInstr::Sh(self.0.into()),
            Op10::STORE_W => return DecodedInstr::Sw(self.0.into()),
            Op10::ADDI => return DecodedInstr::Addi(self.0.into()),
            Op10::SLTI => return DecodedInstr::Slti(self.0.into()),
            Op10::SLTIU => return DecodedInstr::Sltiu(self.0.into()),
            Op10::XORI => return DecodedInstr::Xori(self.0.into()),
            Op10::ORI => return DecodedInstr::Ori(self.0.into()),
            Op10::ANDI => return DecodedInstr::Andi(self.0.into()),
            _ => {}
        }
        match Op17::from_instr(self) {
            Op17::SLLI => return DecodedInstr::Slli(self.0.into()),
            Op17::SRLI => return DecodedInstr::Srli(self.0.into()),
            Op17::SRAI => return DecodedInstr::Srai(self.0.into()),
            Op17::ADD => return DecodedInstr::Add(self.0.into()),
            Op17::SUB => return DecodedInstr::Sub(self.0.into()),
            Op17::SLL => return DecodedInstr::Sll(self.0.into()),
            Op17::SLT => return DecodedInstr::Slt(self.0.into()),
            Op17::SLTU => return DecodedInstr::Sltu(self.0.into()),
            Op17::XOR => return DecodedInstr::Xor(self.0.into()),
            Op17::SRL => return DecodedInstr::Srl(self.0.into()),
            Op17::SRA => return DecodedInstr::Sra(self.0.into()),
            Op17::OR => return DecodedInstr::Or(self.0.into()),
            Op17::AND => return DecodedInstr::And(self.0.into()),
            Op17::MUL => return DecodedInstr::Mul(self.0.into()),
            Op17::MULH => return DecodedInstr::Mulh(self.0.into()),
            Op17::MULHSU => return DecodedInstr::Mulhsu(self.0.into()),
            Op17::MULHU => return DecodedInstr::Mulhu(self.0.into()),
            Op17::DIV => return DecodedInstr::Div(self.0.into()),
            Op17::DIVU => return DecodedInstr::Divu(self.0.into()),
            Op17::REM => return DecodedInstr::Rem(self.0.into()),
            Op17::REMU => return DecodedInstr::Remu(self.0.into()),
            Op17::ANDN => return DecodedInstr::Andn(self.0.into()),
            Op17::BCLR => return DecodedInstr::Bclr(self.0.into()),
            Op17::BEXT => return DecodedInstr::Bext(self.0.into()),
            Op17::BINV => return DecodedInstr::Binv(self.0.into()),
            Op17::BSET => return DecodedInstr::Bset(self.0.into()),
            Op17::CLMUL => return DecodedInstr::Clmul(self.0.into()),
            Op17::CLMULH => return DecodedInstr::Clmulh(self.0.into()),
            Op17::CLMULR => return DecodedInstr::Clmulr(self.0.into()),
            Op17::MAX => return DecodedInstr::Max(self.0.into()),
            Op17::MAXU => return DecodedInstr::Maxu(self.0.into()),
            Op17::MIN => return DecodedInstr::Min(self.0.into()),
            Op17::MINU => return DecodedInstr::Minu(self.0.into()),
            Op17::ORN => return DecodedInstr::Orn(self.0.into()),
            Op17::ROL => return DecodedInstr::Rol(self.0.into()),
            Op17::ROR => return DecodedInstr::Ror(self.0.into()),
            Op17::SH1ADD => return DecodedInstr::Sh1add(self.0.into()),
            Op17::SH2ADD => return DecodedInstr::Sh2add(self.0.into()),
            Op17::SH3ADD => return DecodedInstr::Sh3add(self.0.into()),
            Op17::XNOR => return DecodedInstr::Xnor(self.0.into()),
            Op17::BCLRI => return DecodedInstr::Bclri(self.0.into()),
            Op17::BEXTI => return DecodedInstr::Bexti(self.0.into()),
            Op17::BINVI => return DecodedInstr::Binvi(self.0.into()),
            Op17::BSETI => return DecodedInstr::Bseti(self.0.into()),
            Op17::RORI => return DecodedInstr::Rori(self.0.into()),
            _ => {}
        }
        match Op22::from_instr(self) {
            Op22::CLZ => return DecodedInstr::Clz(self.0.into()),
            Op22::CPOP => return DecodedInstr::Cpop(self.0.into()),
            Op22::CTZ => return DecodedInstr::Ctz(self.0.into()),
            Op22::ORCB => return DecodedInstr::OrcB(self.0.into()),
            Op22::REV8 => return DecodedInstr::Rev8(self.0.into()),
            Op22::SEXTB => return DecodedInstr::SextB(self.0.into()),
            Op22::SEXTH => return DecodedInstr::SextH(self.0.into()),
            Op22::ZEXT_H => return DecodedInstr::ZextH(self.0.into()),
            _ => {}
        }
        DecodedInstr::Unknown(self.0)
    }
    /// Returns true if this instruction is a 16-bit compressed instruction.
    pub fn is_compressed(self) -> bool {
        (self.0 & OP_MASK_32) != OP_MASK_32
    }
}
impl From<u32> for Instr {
    fn from(value: u32) -> Self {
        Self(value)
    }
}
impl From<u16> for Instr {
    fn from(value: u16) -> Self {
        Self(value.into())
    }
}
const fn sign_extend_i16(val: u16, msb: u16) -> i16 {
    let sh = 15 - msb;
    ((val << sh).cast_signed()) >> sh
}
const fn sign_extend_i32(val: u32, msb: u32) -> i32 {
    let sh = 31 - msb;
    ((val << sh).cast_signed()) >> sh
}
const OP_MASK_32: u32 = 0x3;
#[derive(Debug, Eq, PartialEq)]
struct Opcode(pub u32);
impl Opcode {
    #![allow(unused)]
    const LOAD: Self = Self(OP_MASK_32);
    const LOAD_FP: Self = Self((1 << 2) | OP_MASK_32);
    const CUSTOM_0: Self = Self((2 << 2) | OP_MASK_32);
    const MISC_MEM: Self = Self((3 << 2) | OP_MASK_32);
    const OP_IMM: Self = Self((4 << 2) | OP_MASK_32);
    const AUIPC: Self = Self((5 << 2) | OP_MASK_32);
    const IMM_32: Self = Self((6 << 2) | OP_MASK_32);
    const STORE: Self = Self((8 << 2) | OP_MASK_32);
    const STORE_FP: Self = Self((9 << 2) | OP_MASK_32);
    const CUSTOM_1: Self = Self((10 << 2) | OP_MASK_32);
    const AMO: Self = Self((11 << 2) | OP_MASK_32);
    const OP: Self = Self((12 << 2) | OP_MASK_32);
    const LUI: Self = Self((13 << 2) | OP_MASK_32);
    const OP_32: Self = Self((14 << 2) | OP_MASK_32);
    const MADD: Self = Self((16 << 2) | OP_MASK_32);
    const MSUB: Self = Self((17 << 2) | OP_MASK_32);
    const NMSUB: Self = Self((18 << 2) | OP_MASK_32);
    const NMADD: Self = Self((19 << 2) | OP_MASK_32);
    const OP_FP: Self = Self((20 << 2) | OP_MASK_32);
    const BRANCH: Self = Self((24 << 2) | OP_MASK_32);
    const JALR: Self = Self((25 << 2) | OP_MASK_32);
    const JAL: Self = Self((27 << 2) | OP_MASK_32);
    const SYSTEM: Self = Self((28 << 2) | OP_MASK_32);
    pub const fn from_bits(val: u32) -> Self {
        Self(val & 0x7f)
    }
    pub const fn into_bits(self) -> u32 {
        self.0
    }
    pub const fn funct3(self, funct3: u32) -> u32 {
        ((funct3 & 0x7) << 12) | (self.0 & 0x7f)
    }
    pub const fn funct10(self, funct10: u32) -> u32 {
        ((funct10 >> 3) << 25) | ((funct10 & 0x7) << 12) | (self.0 & 0x7f)
    }
    pub const fn funct15(self, funct15: u32) -> u32 {
        ((funct15 >> 3) << 20) | ((funct15 & 0x7) << 12) | (self.0 & 0x7f)
    }
}
/// An identifier for instructions that don't have any additional constant bits
/// (funct3, etc) beyond opcode.
#[derive(Eq, PartialEq)]
struct Op7(pub u32);
impl Op7 {
    pub const fn from_instr(instr: Instr) -> Self {
        Self(instr.0 & 0b0111_1111)
    }
    const LUI: Self = Self(Opcode::LUI.0);
    const AUIPC: Self = Self(Opcode::AUIPC.0);
    const JAL: Self = Self(Opcode::JAL.0);
}
/// The identifier for instructions that contain funct3 constant bits.
#[derive(Eq, PartialEq)]
struct Op10(pub u32);
impl Op10 {
    pub const fn from_instr(instr: Instr) -> Op10 {
        Self(instr.0 & 0b111_0000_0111_1111)
    }
    pub const JALR: Self = Self(Opcode::JALR.funct3(0));
    pub const BEQ: Self = Self(Opcode::BRANCH.funct3(FUNCT3_BRANCH_EQ));
    pub const BNE: Self = Self(Opcode::BRANCH.funct3(FUNCT3_BRANCH_NE));
    pub const BLT: Self = Self(Opcode::BRANCH.funct3(FUNCT3_BRANCH_LT));
    pub const BGE: Self = Self(Opcode::BRANCH.funct3(FUNCT3_BRANCH_GE));
    pub const BLTU: Self = Self(Opcode::BRANCH.funct3(FUNCT3_BRANCH_LTU));
    pub const BGEU: Self = Self(Opcode::BRANCH.funct3(FUNCT3_BRANCH_GEU));
    pub const LOAD_B: Self = Self(Opcode::LOAD.funct3(FUNCT3_LOAD_B));
    pub const LOAD_H: Self = Self(Opcode::LOAD.funct3(FUNCT3_LOAD_H));
    pub const LOAD_W: Self = Self(Opcode::LOAD.funct3(FUNCT3_LOAD_W));
    pub const LOAD_BU: Self = Self(Opcode::LOAD.funct3(FUNCT3_LOAD_BU));
    pub const LOAD_HU: Self = Self(Opcode::LOAD.funct3(FUNCT3_LOAD_HU));
    pub const STORE_B: Self = Self(Opcode::STORE.funct3(FUNCT3_STORE_B));
    pub const STORE_H: Self = Self(Opcode::STORE.funct3(FUNCT3_STORE_H));
    pub const STORE_W: Self = Self(Opcode::STORE.funct3(FUNCT3_STORE_W));
    pub const ADDI: Self = Self(Opcode::OP_IMM.funct3(FUNCT3_OP_ADDI));
    pub const SLTI: Self = Self(Opcode::OP_IMM.funct3(FUNCT3_OP_SLTI));
    pub const SLTIU: Self = Self(Opcode::OP_IMM.funct3(FUNCT3_OP_SLTIU));
    pub const XORI: Self = Self(Opcode::OP_IMM.funct3(FUNCT3_OP_XORI));
    pub const ORI: Self = Self(Opcode::OP_IMM.funct3(FUNCT3_OP_ORI));
    pub const ANDI: Self = Self(Opcode::OP_IMM.funct3(FUNCT3_OP_ANDI));
}
/// The identifier for instructions that contain funct3 and funct7 constant
/// bits.
#[derive(Eq, PartialEq)]
struct Op17(pub u32);
impl Op17 {
    #![allow(clippy::unusual_byte_groupings)]
    pub const fn from_instr(instr: Instr) -> Self {
        Self(instr.0 & 0b1111_1110_0000_0000_0111_0000_0111_1111)
    }
    pub const SLLI: Self = Self(Opcode::OP_IMM.funct10(FUNCT10_SLLI));
    pub const SRLI: Self = Self(Opcode::OP_IMM.funct10(FUNCT10_SRLI));
    pub const SRAI: Self = Self(Opcode::OP_IMM.funct10(FUNCT10_SRAI));
    pub const BCLRI: Self = Self(Opcode::OP_IMM.funct10(0b0100100_001));
    pub const BEXTI: Self = Self(Opcode::OP_IMM.funct10(0b0100100_101));
    pub const BINVI: Self = Self(Opcode::OP_IMM.funct10(0b0110100_001));
    pub const BSETI: Self = Self(Opcode::OP_IMM.funct10(0b0010100_001));
    pub const RORI: Self = Self(Opcode::OP_IMM.funct10(0b0110000_101));
    pub const ADD: Self = Self(Opcode::OP.funct10(FUNCT10_ADD));
    pub const SUB: Self = Self(Opcode::OP.funct10(FUNCT10_SUB));
    pub const SLL: Self = Self(Opcode::OP.funct10(FUNCT10_SLL));
    pub const SLT: Self = Self(Opcode::OP.funct10(FUNCT10_SLT));
    pub const SLTU: Self = Self(Opcode::OP.funct10(FUNCT10_SLTU));
    pub const XOR: Self = Self(Opcode::OP.funct10(FUNCT10_XOR));
    pub const SRL: Self = Self(Opcode::OP.funct10(FUNCT10_SRL));
    pub const SRA: Self = Self(Opcode::OP.funct10(FUNCT10_SRA));
    pub const OR: Self = Self(Opcode::OP.funct10(FUNCT10_OR));
    pub const AND: Self = Self(Opcode::OP.funct10(FUNCT10_AND));
    pub const MUL: Self = Self(Opcode::OP.funct10(FUNCT10_MUL));
    pub const MULH: Self = Self(Opcode::OP.funct10(FUNCT10_MULH));
    pub const MULHSU: Self = Self(Opcode::OP.funct10(FUNCT10_MULHSU));
    pub const MULHU: Self = Self(Opcode::OP.funct10(FUNCT10_MULHU));
    pub const DIV: Self = Self(Opcode::OP.funct10(FUNCT10_DIV));
    pub const DIVU: Self = Self(Opcode::OP.funct10(FUNCT10_DIVU));
    pub const REM: Self = Self(Opcode::OP.funct10(FUNCT10_REM));
    pub const REMU: Self = Self(Opcode::OP.funct10(FUNCT10_REMU));
    pub const ANDN: Self = Self(Opcode::OP.funct10(0b0100000_111));
    pub const BCLR: Self = Self(Opcode::OP.funct10(0b0100100_001));
    pub const BEXT: Self = Self(Opcode::OP.funct10(0b0100100_101));
    pub const BINV: Self = Self(Opcode::OP.funct10(0b0110100_001));
    pub const BSET: Self = Self(Opcode::OP.funct10(0b0010100_001));
    pub const CLMUL: Self = Self(Opcode::OP.funct10(0b0000101_001));
    pub const CLMULH: Self = Self(Opcode::OP.funct10(0b0000101_011));
    pub const CLMULR: Self = Self(Opcode::OP.funct10(0b0000101_010));
    pub const MAX: Self = Self(Opcode::OP.funct10(0b0000101_110));
    pub const MAXU: Self = Self(Opcode::OP.funct10(0b0000101_111));
    pub const MIN: Self = Self(Opcode::OP.funct10(0b0000101_100));
    pub const MINU: Self = Self(Opcode::OP.funct10(0b0000101_101));
    pub const ORN: Self = Self(Opcode::OP.funct10(0b0100000_110));
    pub const ROL: Self = Self(Opcode::OP.funct10(0b0110000_001));
    pub const ROR: Self = Self(Opcode::OP.funct10(0b0110000_101));
    pub const SH1ADD: Self = Self(Opcode::OP.funct10(0b0010000_010));
    pub const SH2ADD: Self = Self(Opcode::OP.funct10(0b0010000_100));
    pub const SH3ADD: Self = Self(Opcode::OP.funct10(0b0010000_110));
    pub const XNOR: Self = Self(Opcode::OP.funct10(0b0100000_100));
}
/// The identifier for instructions that contain 22 constant bits.
#[derive(Eq, PartialEq)]
struct Op22(pub u32);
impl Op22 {
    #![allow(clippy::unusual_byte_groupings)]
    pub const fn from_instr(instr: Instr) -> Self {
        Self(instr.0 & 0b1111_1111_1111_0000_0111_0000_0111_1111)
    }
    pub const CLZ: Self = Self(Opcode::OP_IMM.funct15(0b0110000_00000_001));
    pub const CPOP: Self = Self(Opcode::OP_IMM.funct15(0b0110000_00010_001));
    pub const CTZ: Self = Self(Opcode::OP_IMM.funct15(0b0110000_00001_001));
    pub const ORCB: Self = Self(Opcode::OP_IMM.funct15(0b0010100_00111_101));
    pub const REV8: Self = Self(Opcode::OP_IMM.funct15(0b0110100_11000_101));
    pub const SEXTB: Self = Self(Opcode::OP_IMM.funct15(0b0110000_00100_001));
    pub const SEXTH: Self = Self(Opcode::OP_IMM.funct15(0b0110000_00101_001));
    pub const ZEXT_H: Self = Self(Opcode::OP.funct15(0b0000100_00000_100));
}
#[allow(unused)]
pub enum InstrType {
    R(Instr32R),
    I(Instr32I),
    IS(Instr32IS),
    IB(Instr32IB),
    S(Instr32S),
    B(Instr32B),
    U(Instr32U),
    J(Instr32J),
    Const(u32),
    Unknown(u32),
}
impl InstrType {
    pub fn rd(&self) -> Option<Reg> {
        match self {
            Self::R(i) => Some(i.rd()),
            Self::I(i) => Some(i.rd()),
            Self::IS(i) => Some(i.rd()),
            Self::IB(i) => Some(i.rd()),
            Self::S(_) => None,
            Self::B(_) => None,
            Self::U(i) => Some(i.rd()),
            Self::J(i) => Some(i.rd()),
            Self::Const(_) => None,
            Self::Unknown(_) => None,
        }
    }
}
#[derive(Clone, Copy)]
pub enum DecodedInstr {
    Unknown(u32),
    Lui(Instr32U),
    Auipc(Instr32U),
    Jal(Instr32J),
    Jalr(Instr32I),
    Beq(Instr32B),
    Bne(Instr32B),
    Blt(Instr32B),
    Bge(Instr32B),
    Bltu(Instr32B),
    Bgeu(Instr32B),
    Lb(Instr32I),
    Lh(Instr32I),
    Lw(Instr32I),
    Lbu(Instr32I),
    Lhu(Instr32I),
    Sb(Instr32S),
    Sh(Instr32S),
    Sw(Instr32S),
    Addi(Instr32I),
    Slti(Instr32I),
    Sltiu(Instr32I),
    Xori(Instr32I),
    Ori(Instr32I),
    Andi(Instr32I),
    Slli(Instr32IS),
    Srli(Instr32IS),
    Srai(Instr32IS),
    Bclri(Instr32IS),
    Bexti(Instr32IS),
    Binvi(Instr32IS),
    Bseti(Instr32IS),
    Rori(Instr32IS),
    Clz(Instr32IB),
    Cpop(Instr32IB),
    Ctz(Instr32IB),
    OrcB(Instr32IB),
    Rev8(Instr32IB),
    SextB(Instr32IB),
    SextH(Instr32IB),
    ZextH(Instr32IB),
    Add(Instr32R),
    Sub(Instr32R),
    Sll(Instr32R),
    Slt(Instr32R),
    Sltu(Instr32R),
    Xor(Instr32R),
    Srl(Instr32R),
    Sra(Instr32R),
    Or(Instr32R),
    And(Instr32R),
    Mul(Instr32R),
    Mulh(Instr32R),
    Mulhsu(Instr32R),
    Mulhu(Instr32R),
    Div(Instr32R),
    Divu(Instr32R),
    Rem(Instr32R),
    Remu(Instr32R),
    Andn(Instr32R),
    Bclr(Instr32R),
    Bext(Instr32R),
    Binv(Instr32R),
    Bset(Instr32R),
    Clmul(Instr32R),
    Clmulh(Instr32R),
    Clmulr(Instr32R),
    Max(Instr32R),
    Maxu(Instr32R),
    Min(Instr32R),
    Minu(Instr32R),
    Orn(Instr32R),
    Rol(Instr32R),
    Ror(Instr32R),
    Sh1add(Instr32R),
    Sh2add(Instr32R),
    Sh3add(Instr32R),
    Xnor(Instr32R),
    #[allow(dead_code)]
    FenceTso,
    #[allow(dead_code)]
    Pause,
    #[allow(dead_code)]
    Ecall,
    #[allow(dead_code)]
    Ebreak,
}
impl DecodedInstr {
    pub fn ty(&self) -> InstrType {
        #![allow(clippy::unusual_byte_groupings)]
        match self {
            Self::Lui(i) => InstrType::U(*i),
            Self::Auipc(i) => InstrType::U(*i),
            Self::Jal(i) => InstrType::J(*i),
            Self::Jalr(i) => InstrType::I(*i),
            Self::Beq(i) => InstrType::B(*i),
            Self::Bne(i) => InstrType::B(*i),
            Self::Blt(i) => InstrType::B(*i),
            Self::Bge(i) => InstrType::B(*i),
            Self::Bltu(i) => InstrType::B(*i),
            Self::Bgeu(i) => InstrType::B(*i),
            Self::Lb(i) => InstrType::I(*i),
            Self::Lh(i) => InstrType::I(*i),
            Self::Lw(i) => InstrType::I(*i),
            Self::Lbu(i) => InstrType::I(*i),
            Self::Lhu(i) => InstrType::I(*i),
            Self::Sb(i) => InstrType::S(*i),
            Self::Sh(i) => InstrType::S(*i),
            Self::Sw(i) => InstrType::S(*i),
            Self::Addi(i) => InstrType::I(*i),
            Self::Slti(i) => InstrType::I(*i),
            Self::Sltiu(i) => InstrType::I(*i),
            Self::Xori(i) => InstrType::I(*i),
            Self::Ori(i) => InstrType::I(*i),
            Self::Andi(i) => InstrType::I(*i),
            Self::Slli(i) => InstrType::IS(*i),
            Self::Srli(i) => InstrType::IS(*i),
            Self::Srai(i) => InstrType::IS(*i),
            Self::Bclri(i) => InstrType::IS(*i),
            Self::Bexti(i) => InstrType::IS(*i),
            Self::Binvi(i) => InstrType::IS(*i),
            Self::Bseti(i) => InstrType::IS(*i),
            Self::Rori(i) => InstrType::IS(*i),
            Self::Clz(i) => InstrType::IB(*i),
            Self::Cpop(i) => InstrType::IB(*i),
            Self::Ctz(i) => InstrType::IB(*i),
            Self::OrcB(i) => InstrType::IB(*i),
            Self::Rev8(i) => InstrType::IB(*i),
            Self::SextB(i) => InstrType::IB(*i),
            Self::SextH(i) => InstrType::IB(*i),
            Self::ZextH(i) => InstrType::IB(*i),
            Self::Add(i) => InstrType::R(*i),
            Self::Sub(i) => InstrType::R(*i),
            Self::Sll(i) => InstrType::R(*i),
            Self::Slt(i) => InstrType::R(*i),
            Self::Sltu(i) => InstrType::R(*i),
            Self::Xor(i) => InstrType::R(*i),
            Self::Srl(i) => InstrType::R(*i),
            Self::Sra(i) => InstrType::R(*i),
            Self::Or(i) => InstrType::R(*i),
            Self::And(i) => InstrType::R(*i),
            Self::Mul(i) => InstrType::R(*i),
            Self::Mulh(i) => InstrType::R(*i),
            Self::Mulhsu(i) => InstrType::R(*i),
            Self::Mulhu(i) => InstrType::R(*i),
            Self::Div(i) => InstrType::R(*i),
            Self::Divu(i) => InstrType::R(*i),
            Self::Rem(i) => InstrType::R(*i),
            Self::Remu(i) => InstrType::R(*i),
            Self::Andn(i) => InstrType::R(*i),
            Self::Bclr(i) => InstrType::R(*i),
            Self::Bext(i) => InstrType::R(*i),
            Self::Binv(i) => InstrType::R(*i),
            Self::Bset(i) => InstrType::R(*i),
            Self::Clmul(i) => InstrType::R(*i),
            Self::Clmulh(i) => InstrType::R(*i),
            Self::Clmulr(i) => InstrType::R(*i),
            Self::Max(i) => InstrType::R(*i),
            Self::Maxu(i) => InstrType::R(*i),
            Self::Min(i) => InstrType::R(*i),
            Self::Minu(i) => InstrType::R(*i),
            Self::Orn(i) => InstrType::R(*i),
            Self::Rol(i) => InstrType::R(*i),
            Self::Ror(i) => InstrType::R(*i),
            Self::Sh1add(i) => InstrType::R(*i),
            Self::Sh2add(i) => InstrType::R(*i),
            Self::Sh3add(i) => InstrType::R(*i),
            Self::Xnor(i) => InstrType::R(*i),
            Self::FenceTso => InstrType::Const(0b1000_0011_0011_00000_000_00000_0001111),
            Self::Pause => InstrType::Const(0b0000_0001_0000_00000_000_00000_0001111),
            Self::Ecall => InstrType::Const(0b000000000000_00000_000_00000_1110011),
            Self::Ebreak => InstrType::Const(0b000000000001_00000_000_00000_1110011),
            Self::Unknown(i) => InstrType::Unknown(*i),
        }
    }
    pub fn mnemonic(&self) -> &'static str {
        match self {
            Self::Lui(_) => "lui",
            Self::Auipc(_) => "auipc",
            Self::Jal(_) => "jal",
            Self::Jalr(_) => "jalr",
            Self::Beq(_) => "beq",
            Self::Bne(_) => "bne",
            Self::Blt(_) => "blt",
            Self::Bge(_) => "bge",
            Self::Bltu(_) => "bltu",
            Self::Bgeu(_) => "bgeu",
            Self::Lb(_) => "lb",
            Self::Lh(_) => "lh",
            Self::Lw(_) => "lw",
            Self::Lbu(_) => "lbu",
            Self::Lhu(_) => "lhu",
            Self::Sb(_) => "sb",
            Self::Sh(_) => "sh",
            Self::Sw(_) => "sw",
            Self::Addi(_) => "addi",
            Self::Slti(_) => "slti",
            Self::Sltiu(_) => "sltiu",
            Self::Xori(_) => "xori",
            Self::Ori(_) => "ori",
            Self::Andi(_) => "andi",
            Self::Slli(_) => "slli",
            Self::Srli(_) => "srli",
            Self::Srai(_) => "srai",
            Self::Bclri(_) => "bclri",
            Self::Bexti(_) => "bexti",
            Self::Binvi(_) => "binvi",
            Self::Bseti(_) => "bseti",
            Self::Rori(_) => "rori",
            Self::Clz(_) => "clz",
            Self::Cpop(_) => "cpop",
            Self::Ctz(_) => "ctz",
            Self::OrcB(_) => "orc.b",
            Self::Rev8(_) => "rev8",
            Self::SextB(_) => "sext.b",
            Self::SextH(_) => "sext.h",
            Self::ZextH(_) => "zext.h",
            Self::Add(_) => "add",
            Self::Sub(_) => "sub",
            Self::Sll(_) => "sll",
            Self::Slt(_) => "slt",
            Self::Sltu(_) => "sltu",
            Self::Xor(_) => "xor",
            Self::Srl(_) => "srl",
            Self::Sra(_) => "sra",
            Self::Or(_) => "or",
            Self::And(_) => "and",
            Self::Mul(_) => "mul",
            Self::Mulh(_) => "mulh",
            Self::Mulhsu(_) => "mulhsu",
            Self::Mulhu(_) => "mulhu",
            Self::Div(_) => "div",
            Self::Divu(_) => "divu",
            Self::Rem(_) => "rem",
            Self::Remu(_) => "remu",
            Self::Andn(_) => "andn",
            Self::Bclr(_) => "bclr",
            Self::Bext(_) => "bext",
            Self::Binv(_) => "binv",
            Self::Bset(_) => "bset",
            Self::Clmul(_) => "clmul",
            Self::Clmulh(_) => "clmulh",
            Self::Clmulr(_) => "clmulr",
            Self::Max(_) => "max",
            Self::Maxu(_) => "maxu",
            Self::Min(_) => "min",
            Self::Minu(_) => "minu",
            Self::Orn(_) => "orn",
            Self::Rol(_) => "rol",
            Self::Ror(_) => "ror",
            Self::Sh1add(_) => "sh1add",
            Self::Sh2add(_) => "sh2add",
            Self::Sh3add(_) => "sh3add",
            Self::Xnor(_) => "xnor",
            Self::FenceTso => "fence_tso",
            Self::Pause => "pause",
            Self::Ecall => "ecall",
            Self::Ebreak => "ebreak",
            Self::Unknown(_) => "",
        }
    }
}
impl Display for DecodedInstr {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self.ty() {
            InstrType::B(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::I(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::IB(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::IS(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::S(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::J(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::R(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::U(i) => write!(f, "{:<8} {}", self.mnemonic(), i),
            InstrType::Const(_) => write!(f, "{:<8}", self.mnemonic()),
            InstrType::Unknown(i) => write!(f, "0x{:x}", i),
        }
    }
}
#[bitfield(u32)]
pub struct Instr32R {
    #[bits(7)]
    pub opcode: Opcode,
    #[bits(5)]
    pub rd: Reg,
    #[bits(3)]
    pub funct10_lower: u32,
    #[bits(5)]
    pub rs1: Reg,
    #[bits(5)]
    pub rs2: Reg,
    #[bits(7)]
    pub funct10_upper: u32,
}
impl Instr32R {
    pub fn funct10(&self) -> u32 {
        (self.funct10_upper() << 3) | self.funct10_lower()
    }
    pub fn set_funct10(&mut self, val: u32) {
        self.set_funct10_lower(val & 0x7);
        self.set_funct10_upper(val >> 3)
    }
}
impl Display for Instr32R {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},{},{}", self.rd(), self.rs1(), self.rs2())
    }
}
#[bitfield(u32)]
pub struct Instr32I {
    #[bits(7)]
    pub opcode: Opcode,
    #[bits(5)]
    pub rd: Reg,
    #[bits(3)]
    pub funct3: u32,
    #[bits(5)]
    pub rs1: Reg,
    #[bits(12)]
    pub uimm: u32,
}
impl Instr32I {
    pub fn imm(&self) -> i32 {
        sign_extend_i32(self.uimm(), 11)
    }
    pub fn set_imm(&mut self, val: i32) {
        self.set_uimm(val.cast_unsigned() & 0xfff);
    }
}
impl Display for Instr32I {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        if self.opcode() == Opcode::LOAD {
            write!(f, "{},{}({})", self.rd(), self.imm(), self.rs1())
        } else {
            write!(f, "{},{},{}", self.rd(), self.rs1(), self.imm())
        }
    }
}
#[bitfield(u32)]
pub struct Instr32IS {
    #[bits(7)]
    opcode: Opcode,
    #[bits(5)]
    rd: Reg,
    #[bits(3)]
    funct3: u32,
    #[bits(5)]
    rs1: Reg,
    #[bits(5)]
    uimm: u32,
    #[bits(7)]
    funct7: u32,
}
impl Instr32IS {
    pub const fn funct10(&self) -> u32 {
        (self.funct7() << 3) | self.funct3()
    }
    pub fn set_funct10(&mut self, val: u32) {
        self.set_funct3(val & 0x7);
        self.set_funct7(val >> 3)
    }
    pub fn imm(&self) -> i32 {
        sign_extend_i32(self.uimm(), 4)
    }
}
impl Display for Instr32IS {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},{},{}", self.rd(), self.rs1(), self.uimm())
    }
}
#[bitfield(u32)]
pub struct Instr32IB {
    #[bits(7)]
    opcode: Opcode,
    #[bits(5)]
    rd: Reg,
    #[bits(3)]
    funct3: u32,
    #[bits(5)]
    rs1: Reg,
    #[bits(12)]
    funct12: u32,
}
impl Instr32IB {
    pub const fn funct15(&self) -> u32 {
        (self.funct12() << 3) | self.funct3()
    }
    pub fn set_funct15(&mut self, val: u32) {
        self.set_funct3(val & 0x7);
        self.set_funct12(val >> 3)
    }
}
impl Display for Instr32IB {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},{}", self.rd(), self.rs1())
    }
}
#[bitfield(u32)]
pub struct Instr32S {
    #[bits(7)]
    pub opcode: Opcode,
    #[bits(5)]
    pub imm0: u32,
    #[bits(3)]
    pub funct3: u32,
    #[bits(5)]
    pub rs1: Reg,
    #[bits(5)]
    pub rs2: Reg,
    #[bits(7)]
    pub imm5: u32,
}
impl Instr32S {
    pub const fn uimm(&self) -> u32 {
        (self.imm5() << 5) | self.imm0()
    }
    pub fn set_uimm(&mut self, val: u32) {
        self.set_imm0(val & 0x1f);
        self.set_imm5(val >> 5)
    }
    pub fn imm(&self) -> i32 {
        sign_extend_i32(self.uimm(), 11)
    }
    pub fn set_imm(&mut self, val: i32) {
        self.set_uimm(val.cast_unsigned() & 0xfff);
    }
}
impl Display for Instr32S {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},{}({})", self.rs2(), self.imm(), self.rs1())
    }
}
#[bitfield(u32)]
pub struct Instr32B {
    #[bits(7)]
    opcode: Opcode,
    #[bits(1)]
    imm11: u32,
    #[bits(4)]
    imm1: u32,
    #[bits(3)]
    funct3: u32,
    #[bits(5)]
    rs1: Reg,
    #[bits(5)]
    rs2: Reg,
    #[bits(6)]
    imm5: u32,
    #[bits(1)]
    imm12: u32,
}
impl Instr32B {
    fn uimm(&self) -> u32 {
        (self.imm1() << 1) | (self.imm5() << 5) | (self.imm11() << 11) | (self.imm12() << 12)
    }
    fn set_uimm(&mut self, val: u32) {
        self.set_imm1((val >> 1) & 0xf);
        self.set_imm5((val >> 5) & 0x3f);
        self.set_imm11((val >> 11) & 1);
        self.set_imm12((val >> 12) & 1);
    }
    fn imm(&self) -> i32 {
        sign_extend_i32(self.uimm(), 11)
    }
    fn set_imm(&mut self, val: i32) {
        self.set_uimm(val.cast_unsigned() & 0xfff);
    }
}
impl Display for Instr32B {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},{},{}", self.rs1(), self.rs2(), self.imm())
    }
}
#[bitfield(u32)]
pub struct Instr32J {
    #[bits(7)]
    opcode: Opcode,
    #[bits(5)]
    rd: Reg,
    #[bits(8)]
    imm12: u32,
    #[bits(1)]
    imm11: u32,
    #[bits(10)]
    imm1: u32,
    #[bits(1)]
    imm20: u32,
}
impl Instr32J {
    fn imm(&self) -> i32 {
        sign_extend_i32(self.uimm(), 20)
    }
    fn set_imm(&mut self, val: i32) {
        self.set_uimm(val.cast_unsigned() & 0x1fffff);
    }
    fn uimm(&self) -> u32 {
        (self.imm1() << 1) | (self.imm11() << 11) | (self.imm12() << 12) | (self.imm20() << 20)
    }
    fn set_uimm(&mut self, val: u32) {
        self.set_imm1((val >> 1) & 0x3ff);
        self.set_imm11((val >> 11) & 1);
        self.set_imm12((val >> 12) & 0xff);
        self.set_imm20(val >> 20);
    }
}
impl Display for Instr32J {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},{}", self.rd(), self.imm())
    }
}
#[bitfield(u32)]
pub struct Instr32U {
    #[bits(7)]
    pub opcode: Opcode,
    #[bits(5)]
    pub rd: Reg,
    #[bits(20)]
    pub imm12: u32,
}
impl Instr32U {
    pub fn uimm(&self) -> u32 {
        self.imm12() << 12
    }
    pub fn set_uimm(&mut self, val: u32) {
        self.set_imm12(val >> 12);
    }
    pub fn imm(&self) -> i32 {
        self.uimm().cast_signed()
    }
    pub fn set_imm(&mut self, val: i32) {
        self.set_uimm(val.cast_unsigned())
    }
}
impl Display for Instr32U {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},0x{:x}", self.rd(), self.uimm() >> 12)
    }
}
const FUNCT3_LOAD_B: u32 = 0;
const FUNCT3_LOAD_H: u32 = 1;
const FUNCT3_LOAD_W: u32 = 2;
const FUNCT3_LOAD_BU: u32 = 4;
const FUNCT3_LOAD_HU: u32 = 5;
const FUNCT3_STORE_B: u32 = 0;
const FUNCT3_STORE_H: u32 = 1;
const FUNCT3_STORE_W: u32 = 2;
const FUNCT3_OP_ADDI: u32 = 0;
const FUNCT3_OP_SLTI: u32 = 2;
const FUNCT3_OP_SLTIU: u32 = 3;
const FUNCT3_OP_XORI: u32 = 4;
const FUNCT3_OP_ORI: u32 = 6;
const FUNCT3_OP_ANDI: u32 = 7;
const FUNCT3_OP_SLI: u32 = 1;
#[allow(dead_code)]
const FUNCT3_OP_SRI: u32 = 5;
const FUNCT3_BRANCH_EQ: u32 = 0;
const FUNCT3_BRANCH_NE: u32 = 1;
const FUNCT3_BRANCH_LT: u32 = 4;
const FUNCT3_BRANCH_GE: u32 = 5;
const FUNCT3_BRANCH_LTU: u32 = 6;
const FUNCT3_BRANCH_GEU: u32 = 7;
const FUNCT10_SLLI: u32 = 0b00_0000_0001;
const FUNCT10_SRLI: u32 = 0b00_0000_0101;
const FUNCT10_SRAI: u32 = 0b01_0000_0101;
const FUNCT10_ADD: u32 = 0x000;
const FUNCT10_SUB: u32 = 0x100;
const FUNCT10_SLL: u32 = 0x001;
const FUNCT10_SLT: u32 = 0x002;
const FUNCT10_SLTU: u32 = 0x003;
const FUNCT10_XOR: u32 = 0x004;
const FUNCT10_SRL: u32 = 0x005;
const FUNCT10_SRA: u32 = 0x105;
const FUNCT10_OR: u32 = 0x006;
const FUNCT10_AND: u32 = 0x007;
const FUNCT10_MUL: u32 = 0x008;
const FUNCT10_MULH: u32 = 0x009;
const FUNCT10_MULHSU: u32 = 0x00a;
const FUNCT10_MULHU: u32 = 0x00b;
const FUNCT10_DIV: u32 = 0x00c;
const FUNCT10_DIVU: u32 = 0x00d;
const FUNCT10_REM: u32 = 0x00e;
const FUNCT10_REMU: u32 = 0x00f;
#[derive(Debug, Eq, PartialEq, Clone, Copy)]
pub enum Reg {
    X0 = 0,
    X1Ra = 1,
    X2Sp = 2,
    X3Gp = 3,
    X4Tp = 4,
    X5T0 = 5,
    X6T1 = 6,
    X7T2 = 7,
    X8S0 = 8,
    X9S1 = 9,
    X10A0 = 10,
    X11A1 = 11,
    X12A2 = 12,
    X13A3 = 13,
    X14A4 = 14,
    X15A5 = 15,
    X16A6 = 16,
    X17A7 = 17,
    X18S2 = 18,
    X19S3 = 19,
    X20S4 = 20,
    X21S5 = 21,
    X22S6 = 22,
    X23S7 = 23,
    X24S8 = 24,
    X25S9 = 25,
    X26S10 = 26,
    X27S11 = 27,
    X28T3 = 28,
    X29T4 = 29,
    X30T5 = 30,
    X31T6 = 31,
}
impl Reg {
    const fn from_bits(bits: u8) -> Self {
        match bits & 0x1f {
            0 => Self::X0,
            1 => Self::X1Ra,
            2 => Self::X2Sp,
            3 => Self::X3Gp,
            4 => Self::X4Tp,
            5 => Self::X5T0,
            6 => Self::X6T1,
            7 => Self::X7T2,
            8 => Self::X8S0,
            9 => Self::X9S1,
            10 => Self::X10A0,
            11 => Self::X11A1,
            12 => Self::X12A2,
            13 => Self::X13A3,
            14 => Self::X14A4,
            15 => Self::X15A5,
            16 => Self::X16A6,
            17 => Self::X17A7,
            18 => Self::X18S2,
            19 => Self::X19S3,
            20 => Self::X20S4,
            21 => Self::X21S5,
            22 => Self::X22S6,
            23 => Self::X23S7,
            24 => Self::X24S8,
            25 => Self::X25S9,
            26 => Self::X26S10,
            27 => Self::X27S11,
            28 => Self::X28T3,
            29 => Self::X29T4,
            30 => Self::X30T5,
            31 => Self::X31T6,
            _ => unreachable!(),
        }
    }
    const fn into_bits(self) -> u8 {
        self as u8
    }
    pub const fn abi_mnemonic(self) -> &'static str {
        match self {
            Self::X0 => "x0",
            Self::X1Ra => "ra",
            Self::X2Sp => "sp",
            Self::X3Gp => "gp",
            Self::X4Tp => "tp",
            Self::X5T0 => "t0",
            Self::X6T1 => "t1",
            Self::X7T2 => "t2",
            Self::X8S0 => "s0",
            Self::X9S1 => "s1",
            Self::X10A0 => "a0",
            Self::X11A1 => "a1",
            Self::X12A2 => "a2",
            Self::X13A3 => "a3",
            Self::X14A4 => "a4",
            Self::X15A5 => "a5",
            Self::X16A6 => "a6",
            Self::X17A7 => "a7",
            Self::X18S2 => "s2",
            Self::X19S3 => "s3",
            Self::X20S4 => "s4",
            Self::X21S5 => "s5",
            Self::X22S6 => "s6",
            Self::X23S7 => "s7",
            Self::X24S8 => "s8",
            Self::X25S9 => "s9",
            Self::X26S10 => "s10",
            Self::X27S11 => "s11",
            Self::X28T3 => "t3",
            Self::X29T4 => "t4",
            Self::X30T5 => "t5",
            Self::X31T6 => "t6",
        }
    }
}
impl Display for Reg {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(self.abi_mnemonic())
    }
}
impl From<Reg> for usize {
    fn from(value: Reg) -> Self {
        value as usize
    }
}
#[cfg(test)]
mod test {
    use crate::riscv::Instr;
    #[test]
    fn test() {
        assert_eq!(Instr(0x02f807b3).decode().to_string(), "mul      a5,a6,a5");
        assert_eq!(Instr(0x010107b7).decode().to_string(), "lui      a5,0x1010");
        assert_eq!(Instr(0x10178793).decode().to_string(), "addi     a5,a5,257");
        assert_eq!(Instr(0x0141).decode().to_string(), "addi     sp,sp,16");
        assert_eq!(Instr(0x00a92223).decode().to_string(), "sw       a0,4(s2)",);
        assert_eq!(Instr(0xc31c).decode().to_string(), "sw       a5,0(a4)");
        assert_eq!(Instr(0x1141).decode().to_string(), "addi     sp,sp,-16");
        assert_eq!(Instr(0x00e68c63).decode().to_string(), "beq      a3,a4,24");
        assert_eq!(Instr(0xca81).decode().to_string(), "beq      a3,x0,16");
        assert_eq!(Instr(0x8082).decode().to_string(), "jalr     x0,ra,0");
    }
}
