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

//! # Proc macros for RISC-V support in the kernel
//!
//! # Exceptions
//!
//! Two attribute macros ([`kernel_only_exception`] and [`user_space_exception`])
//! are provided for generating exception wrappers.  They have the same syntax
//! and are intended to be conditionally imported based on the kernel's
//! `user_space` feature:
//!
//! ```
//! #[cfg(not(feature = "user_space"))]
//! pub(crate) use riscv_macro::kernel_only_exception as exception;
//! #[cfg(feature = "user_space")]
//! pub(crate) use riscv_macro::user_space_exception as exception;
//! ```
//!
//! To use the macros, an exception name must be provided.  This should match
//! the symbol name for the main trap handler (aka `_start_trap` for `riscv-rt`)
//! ```
//! #[exception(exception = "_start_trap")]
//! #[no_mangle]
//! unsafe extern "C" fn trap_handler(mcause: MCauseVal, mepc: usize, frame: &mut TrapFrame) {
//! }
//! ```

use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::parse::{Parse, ParseStream, Result};
use syn::punctuated::Punctuated;
use syn::{parse_macro_input, Ident, ItemFn, Token};

#[derive(Eq, PartialEq, Hash, Debug)]
enum KernelMode {
    UserSpace,
    KernelOnly,
}

#[derive(Eq, PartialEq, Hash, Debug)]
enum FrameType {
    UserSpace,
    Kernel,
}

#[derive(Eq, PartialEq, Hash, Debug)]
enum Attribute {
    Exception(String),
}

impl Attribute {
    fn parse_value(input: ParseStream) -> Result<String> {
        let _ = input.parse::<Token![=]>()?;
        let value = input.parse::<syn::LitStr>()?;
        Ok(value.value())
    }
}

impl Parse for Attribute {
    fn parse(input: ParseStream) -> Result<Self> {
        let name = input.parse::<Ident>()?;
        let name_str = name.to_string();
        match name_str.as_str() {
            "exception" => {
                let value = Self::parse_value(input)?;
                Ok(Attribute::Exception(value))
            }
            _ => Err(input.error(format!("unknown attribute {name_str}"))),
        }
    }
}

struct Attributes {
    exception: String,
}

impl Parse for Attributes {
    fn parse(input: ParseStream) -> Result<Self> {
        let attributes = Punctuated::<Attribute, Token![,]>::parse_terminated(input)?;
        let mut exception = None;
        for attr in attributes {
            match attr {
                Attribute::Exception(name) => exception = Some(name),
            }
        }

        let exception = exception.ok_or_else(|| input.error("missing exception name"))?;

        Ok(Attributes { exception })
    }
}

enum Register {
    GeneralPurpose(&'static str),
    UserSpace(&'static str),
    Csr(&'static str, &'static str),
    StackPointer,
}

const REGS: &[(Register, usize)] = &[
    (Register::Csr("mepc", "a1"), 0x00),
    (Register::Csr("mstatus", "t0"), 0x04),
    (Register::GeneralPurpose("ra"), 0x08),
    (Register::GeneralPurpose("a0"), 0x0c),
    (Register::GeneralPurpose("a1"), 0x10),
    (Register::GeneralPurpose("a2"), 0x14),
    (Register::GeneralPurpose("a3"), 0x18),
    (Register::GeneralPurpose("a4"), 0x1c),
    (Register::GeneralPurpose("a5"), 0x20),
    (Register::GeneralPurpose("a6"), 0x24),
    (Register::GeneralPurpose("a7"), 0x28),
    (Register::GeneralPurpose("t0"), 0x2c),
    (Register::GeneralPurpose("t1"), 0x30),
    (Register::GeneralPurpose("t2"), 0x34),
    (Register::GeneralPurpose("t3"), 0x38),
    (Register::GeneralPurpose("t4"), 0x3c),
    (Register::GeneralPurpose("t5"), 0x40),
    (Register::GeneralPurpose("t6"), 0x44),
    (Register::UserSpace("tp"), 0x48),
    (Register::UserSpace("gp"), 0x4c),
    (Register::StackPointer, 0x50),
];

const STACK_FRAME_LEN: usize = 0x60;

fn general_purpose_regs(
    regs: &[(Register, usize)],
) -> impl DoubleEndedIterator<Item = (&'static str, usize)> + use<'_> {
    regs.iter().filter_map(|reg| match reg.0 {
        Register::GeneralPurpose(name) => Some((name, reg.1)),
        _ => None,
    })
}

fn csr_regs(
    regs: &[(Register, usize)],
) -> impl DoubleEndedIterator<Item = (&'static str, &'static str, usize)> + use<'_> {
    regs.iter().filter_map(|reg| match reg.0 {
        Register::Csr(name, temp_reg) => Some((name, temp_reg, reg.1)),
        _ => None,
    })
}

fn user_space_regs(
    regs: &[(Register, usize)],
) -> impl DoubleEndedIterator<Item = (&'static str, usize)> + use<'_> {
    regs.iter().filter_map(|reg| match reg.0 {
        Register::UserSpace(name) => Some((name, reg.1)),
        _ => None,
    })
}

fn stack_pointer_offset(regs: &[(Register, usize)]) -> usize {
    regs.iter()
        .filter_map(|reg| match reg.0 {
            Register::StackPointer => Some(reg.1),
            _ => None,
        })
        .next()
        .expect("register set contains stack pointer")
}

fn save_exception_frame(asm: &mut String, frame_type: FrameType, _kernel_mode: &KernelMode) {
    asm.push_str(&format!("addi     sp, sp, -{STACK_FRAME_LEN:#x}\n"));

    for (reg, offset) in general_purpose_regs(REGS).rev() {
        asm.push_str(&format!("sw    {reg}, {offset:#x}(sp)\n"));
    }

    let mut reads = Vec::new();
    let mut stores = Vec::new();
    for (reg, temp_reg, offset) in csr_regs(REGS).rev() {
        reads.push(format!("csrr   {temp_reg}, {reg}\n"));
        stores.push(format!("sw    {temp_reg}, {offset:#x}(sp)\n"));
    }

    // Do all reads before stores for better pipelining.
    for statement in reads.iter().chain(stores.iter()) {
        asm.push_str(statement);
    }

    if frame_type == FrameType::UserSpace {
        for (reg, offset) in user_space_regs(REGS).rev() {
            asm.push_str(&format!("sw    {reg}, {offset:#x}(sp)\n"));
        }

        let sp_offset = stack_pointer_offset(REGS);
        asm.push_str(&format!(
            "
            // Save the stack pointer to the frame and zero out mscratch to
            // signify execution in kernel mode.
            csrrw   t0, mscratch, zero
            sw      t0, {sp_offset:#x}(sp)
            "
        ));
    } else {
        let sp_offset = stack_pointer_offset(REGS);
        asm.push_str(&format!("sw    zero, {sp_offset:#x}(sp)"));
    }
}

fn restore_exception_frame(asm: &mut String, frame_type: FrameType, _kernel_mode: &KernelMode) {
    let mut loads = Vec::new();
    let mut writes = Vec::new();

    if frame_type == FrameType::UserSpace {
        asm.push_str(&format!(
            "
            // Load the kernel stack pointer without the exception frame back
            // into mscratch in preparation for returning from the exception.
            addi    t0, sp, {STACK_FRAME_LEN:#x}
            csrw    mscratch, t0
            "
        ));
        for (reg, offset) in user_space_regs(REGS) {
            asm.push_str(&format!("lw    {reg}, {offset:#x}(sp)\n"));
        }
    }

    for (reg, temp_reg, offset) in csr_regs(REGS) {
        loads.push(format!("lw    {temp_reg}, {offset:#x}(sp)\n"));
        writes.push(format!("csrw   {reg}, {temp_reg}\n"));
    }

    // Do all loads before writes for better pipelining.
    for statement in loads.iter().chain(writes.iter()) {
        asm.push_str(statement);
    }

    for (reg, offset) in general_purpose_regs(REGS) {
        asm.push_str(&format!("lw    {reg}, {offset:#x}(sp)\n"));
    }

    if frame_type == FrameType::UserSpace {
        let sp_offset = stack_pointer_offset(REGS);
        asm.push_str(&format!(
            "
            // Load the user stack pointer as the last time the kernel stack
            // pointer is touched.
            lw       sp, {sp_offset:#x}(sp)
            "
        ));
    } else {
        asm.push_str(&format!("addi     sp, sp, {STACK_FRAME_LEN:#x}\n"));
    }
}

fn call_handler(asm: &mut String, handler_name: &str) {
    // At this point `mepc` will be in `a1`
    asm.push_str(&format!(
        "
        csrr    a0, mcause
        mv      a2, sp

        // Call into the handler with:
        // a0: mcause
        // a1: mepc
        // a2: &TrapFrame
        call    {handler_name}
        "
    ));
}

fn exception_handler(asm: &mut String, kernel_mode: &KernelMode, handler_name: &str) {
    save_exception_frame(asm, FrameType::Kernel, kernel_mode);
    call_handler(asm, handler_name);
    restore_exception_frame(asm, FrameType::Kernel, kernel_mode);

    asm.push_str("mret\n");
}

fn exception(attr: TokenStream, item: TokenStream, kernel_mode: KernelMode) -> TokenStream {
    let handler = parse_macro_input!(item as ItemFn);
    let attributes = parse_macro_input!(attr as Attributes);

    let exception_ident = format_ident!("{}", attributes.exception);
    let handler_ident = &handler.sig.ident;
    let handler_name = handler_ident.clone().to_string();

    let mut asm = String::new();
    if kernel_mode == KernelMode::UserSpace {
        // When built with user space support, the handler can check if it's in
        // kernel or user space mode by swapping sp with mscratch.  If mscratch
        // contained zero, the handler is in kernel mode.  There are two
        // separate paths for user and kernel handler because user handlers need
        // more state saved and restored.
        asm.push_str(
            "
            // Start by exchanging the current (original) stack pointer with mscratch
            csrrw   sp, mscratch, sp
            bnez    sp, 1f            // If non-zero branch to user handler

            // If mscratch was zero, we're in kernel mode.  Exchange the stack pointer
            // with mscratch again to recover the original stack pointer.
            csrrw   sp, mscratch, sp
            ",
        );
    }
    exception_handler(&mut asm, &kernel_mode, &handler_name);

    if kernel_mode == KernelMode::UserSpace {
        asm.push_str("1:\n");
        exception_handler(&mut asm, &kernel_mode, &handler_name);
    }

    quote! {
        #[no_mangle]
        #[unsafe(naked)]
        #[link_section = ".trap"]
        pub unsafe extern "C" fn #exception_ident() -> ! {
            unsafe {
                core::arch::naked_asm!(#asm)
            }
        }
        // Compile time assert that the handler function signature matches.
        const _: crate::arch::riscv::exceptions::ExceptionHandler = #handler_ident;

        #[link_section = ".trap"]
        #handler
    }
    .into()
}

/// Generate an exception wrapper with kernel only support.
#[proc_macro_attribute]
pub fn kernel_only_exception(attr: TokenStream, item: TokenStream) -> TokenStream {
    exception(attr, item, KernelMode::KernelOnly)
}

/// Generate an exception wrapper with user-space support.
#[proc_macro_attribute]
pub fn user_space_exception(attr: TokenStream, item: TokenStream) -> TokenStream {
    exception(attr, item, KernelMode::UserSpace)
}
