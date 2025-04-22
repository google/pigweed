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

//! # Proc macros for ARM Cortex M support in the kernel
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
//! pub(crate) use arm_cortex_m_macro::kernel_only_exception as exception;
//! #[cfg(feature = "user_space")]
//! pub(crate) use arm_cortex_m_macro::user_space_exception as exception;
//! ```
//!
//! To use the macros, an exception name must be provided:
//! ```
//! #[exception(exception = "HardFault")]
//! #[no_mangle]
//! extern "C" fn hard_fault(frame: *mut FullExceptionFrame) ->  *mut FullExceptionFrame {
//! }
//! ```
//!
//! An optional, `disable_interrupts` attribute can be passed to disable
//! interrupts while invoking the handler:
//! ```
//! #[exception(exception = "PendSV", disable_interrupts)]
//! #[no_mangle]
//! extern "C" fn pendsv(frame: *mut FullExceptionFrame) -> *mut FullExceptionFrame {
//! ```

use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parse::{Parse, ParseStream, Result},
    parse_macro_input,
    punctuated::Punctuated,
    Ident, ItemFn, Token,
};

#[derive(Eq, PartialEq, Hash, Debug)]
enum KernelMode {
    UserSpace,
    KernelOnly,
}

impl KernelMode {
    fn save_psp_needed(&self) -> bool {
        *self == Self::UserSpace
    }
}

#[derive(Eq, PartialEq, Hash, Debug)]
enum Attribute {
    Exception(String),
    DisableInterrupts,
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
            "disable_interrupts" => Ok(Attribute::DisableInterrupts),
            _ => Err(input.error(format!("unknown attribute {}", name_str))),
        }
    }
}

struct Attributes {
    exception: String,
    disable_interrupts: bool,
}

impl Parse for Attributes {
    fn parse(input: ParseStream) -> Result<Self> {
        let attributes = Punctuated::<Attribute, Token![,]>::parse_terminated(input)?;
        let mut exception = None;
        let mut disable_interrupts = false;
        for attr in attributes {
            match attr {
                Attribute::Exception(name) => exception = Some(name),
                Attribute::DisableInterrupts => disable_interrupts = true,
            }
        }

        let exception = exception.ok_or_else(|| input.error("missing exception name"))?;

        Ok(Attributes {
            exception,
            disable_interrupts,
        })
    }
}

fn save_exception_frame(asm: &mut String, kernel_mode: &KernelMode) {
    asm.push_str("// save the additional registers\n");
    if kernel_mode.save_psp_needed() {
        asm.push_str(
            "
            mrs     r0, psp
            push    {{ r0, lr }}
            push    {{ r4 - r11 }}
            mov     r0, sp
            ",
        );
    } else {
        asm.push_str(
            "
            push    {{ r4 - r11, lr }}
            mov     r0, sp
            sub     sp, 4   // Align stack to 8 bytes
            ",
        );
    }
}

fn restore_exception_frame(asm: &mut String, kernel_mode: &KernelMode) {
    if kernel_mode.save_psp_needed() {
        asm.push_str(
            "
            mov     sp, r0
            pop     {{ r4 - r11 }}
            pop     {{r0}}
            msr     psp, r0
            pop     {{pc}}
    ",
        );
    } else {
        asm.push_str(
            "
            mov     sp, r0
            pop     {{ r4 - r11, pc }}
    ",
        );
    }
}

fn disable_interrupts(asm: &mut String) {
    asm.push_str("cpsid   i\n");
}

fn enable_interrupts(asm: &mut String) {
    asm.push_str("cpsie   i\n");
}

fn exception(attr: TokenStream, item: TokenStream, kernel_mode: KernelMode) -> TokenStream {
    let handler = parse_macro_input!(item as ItemFn);
    let attributes = parse_macro_input!(attr as Attributes);

    let exception_ident = format_ident!("{}", attributes.exception);
    let handler_name = handler.sig.ident.to_string();

    let mut asm = String::new();
    save_exception_frame(&mut asm, &kernel_mode);

    if attributes.disable_interrupts {
        disable_interrupts(&mut asm);
    }

    asm.push_str(&format!("bl     {}\n", handler_name));

    if attributes.disable_interrupts {
        enable_interrupts(&mut asm);
    }

    restore_exception_frame(&mut asm, &kernel_mode);

    quote! {
        #[no_mangle]
        #[naked]
        pub unsafe extern "C" fn #exception_ident() -> ! {
            unsafe {
                core::arch::naked_asm!(#asm)
            }
        }
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
