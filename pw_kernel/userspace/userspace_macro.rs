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
use proc_macro::TokenStream;
use proc_macro2::Span;
use quote::quote;
use syn::spanned::Spanned;
use syn::{parse, Ident, ItemFn, ReturnType, Type, Visibility};

fn validate_and_set_entry_ident(
    args: TokenStream,
    input: TokenStream,
) -> Result<ItemFn, TokenStream> {
    let mut f = match parse::<ItemFn>(input) {
        Ok(item_fn) => item_fn,
        Err(e) => return Err(e.to_compile_error().into()),
    };

    if !args.is_empty() {
        return Err(
            parse::Error::new(Span::call_site(), "`#[entry]` accepts no arguments")
                .to_compile_error()
                .into(),
        );
    }

    // check the function signature
    let valid_signature = f.sig.constness.is_none()
        && f.sig.asyncness.is_none()
        && f.vis == Visibility::Inherited
        && f.sig.abi.is_none()
        && f.sig.inputs.is_empty()
        && f.sig.generics.params.is_empty()
        && f.sig.generics.where_clause.is_none()
        && f.sig.variadic.is_none()
        && match f.sig.output {
            ReturnType::Default => false,
            ReturnType::Type(_, ref ty) => {
                matches!(**ty, Type::Never(_))
            }
        };

    if !valid_signature {
        return Err(parse::Error::new(
            f.span(),
            "`#[entry]` function must have signature `fn() -> !`",
        )
        .to_compile_error()
        .into());
    }

    f.sig.ident = Ident::new(&format!("_start_{}", f.sig.ident), Span::call_site());
    Ok(f)
}

#[proc_macro_attribute]
pub fn arm_cortex_m_entry(args: TokenStream, input: TokenStream) -> TokenStream {
    let f = validate_and_set_entry_ident(args, input).unwrap();

    let asm = include_str!("arm_cortex_m/entry.s");
    quote!(
        use core::arch::global_asm;
        global_asm!(#asm, options(raw));

        #[no_mangle]
        #[export_name = "main"]
        extern "C" #f
    )
    .into()
}

#[proc_macro_attribute]
pub fn riscv_entry(args: TokenStream, input: TokenStream) -> TokenStream {
    let f = validate_and_set_entry_ident(args, input).unwrap();

    let asm = include_str!("riscv/entry.s");
    quote!(
        use core::arch::global_asm;
        global_asm!(#asm, options(raw));

        #[no_mangle]
        #[export_name = "main"]
        extern "C" #f
    )
    .into()
}
