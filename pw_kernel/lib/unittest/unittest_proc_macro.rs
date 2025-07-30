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
use quote::{format_ident, quote};
use syn::{Ident, ItemFn, parse_macro_input};

#[proc_macro_attribute]
pub fn test(attr: TokenStream, item: TokenStream) -> TokenStream {
    let item: ItemFn = parse_macro_input!(item as ItemFn);
    let fn_ident = item.sig.ident.clone();
    let fn_name = item.sig.ident.to_string();
    let ctor_fn_ident = format_ident!("__mz_unittest_ctor_fn_{}__", fn_name);
    let ctor_ident = format_ident!("__mz_unittest_ctor_{}__", fn_name);
    let desc_ident = format_ident!("__MZ_UNITTEST_DESC_{}__", fn_name.to_uppercase());

    struct Args {
        needs_kernel: bool,
    }

    impl syn::parse::Parse for Args {
        fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
            if input.is_empty() {
                return Ok(Args {
                    needs_kernel: false,
                });
            }

            let needs_kernel: Ident = input.parse()?;
            if needs_kernel != "needs_kernel" {
                return Err(syn::Error::new_spanned(
                    &needs_kernel,
                    "unsupported, expected `needs_kernel`",
                ));
            }

            Ok(Args { needs_kernel: true })
        }
    }

    let args = parse_macro_input!(attr as Args);
    let test_set = if args.needs_kernel {
        quote! { unittest::TestSet::Kernel }
    } else {
        quote! { unittest::TestSet::BareMetal }
    };

    quote! {
        // We put these in a block so they can't be referenced from other code,
        // which ensures that functions defined here can't be called from
        // elsewhere.
        const _: () = {
            static mut #desc_ident: unittest::Test = unittest::Test::new(
                #fn_name,
                #fn_ident,
                #test_set,
            );

            extern "C" fn #ctor_fn_ident() -> usize {
                // SAFETY: This function is defined in a block, so it is not
                // callable from safe code outside of this block. By linker
                // contract, `#ctor_ident` below is only called during
                // initialization, and is only called once. Thus, this function
                // is only called once. Furthermore, we don't reference
                // `#desc_ident` elsewhere. Thus, this is the only reference to
                // `#desc_ident` that will ever happen in this program, and so
                // it is acceptable to take a mutable reference that will live
                // for the rest of the program's lifetime.
                let desc = unsafe { &mut #desc_ident };
                // SAFETY: As described above, this function will only be called
                // during initialization. At that point, execution is
                // single-threaded. Thus, this call to `add_test` will not
                // overlap with any other code executing.
                unsafe { unittest::add_test(desc) };
                0
            }

            #[used]
            #[cfg_attr(target_os = "linux", unsafe(link_section = ".init_array"))]
            #[cfg_attr(target_os = "none", unsafe(link_section = ".init_array"))]
            #[cfg_attr(target_vendor = "apple", unsafe(link_section = "__DATA,__mod_init_func"))]
            #[cfg_attr(windows, unsafe(link_section = ".CRT$XCU"))]
            static #ctor_ident: extern "C" fn() -> usize = #ctor_fn_ident;
        };

        #item
    }
    .into()
}
