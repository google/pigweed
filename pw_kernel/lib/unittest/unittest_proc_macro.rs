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
use syn::{parse_macro_input, ItemFn};

#[proc_macro_attribute]
pub fn test(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let item: ItemFn = parse_macro_input!(item as ItemFn);
    let fn_ident = item.sig.ident.clone();
    let fn_name = item.sig.ident.to_string();
    let ctor_fn_ident = format_ident!("__mz_unittest_ctor_fn_{}__", fn_name);
    let ctor_ident = format_ident!("__mz_unittest_ctor_{}__", fn_name);
    let desc_ident = format_ident!("__MZ_UNITTEST_DESC_{}__", fn_name.to_uppercase());
    quote! {
        static mut #desc_ident: unittest::TestDescAndFn = unittest::TestDescAndFn::new(
            unittest::TestDesc{
                name: #fn_name,
            },
             unittest::TestFn::StaticTestFn(#fn_ident),
        );

        extern "C" fn #ctor_fn_ident() -> usize {
            use core::ptr::addr_of_mut;
            // Safety: We're only ever mutating this at constructor time which
            // is single threaded.
            let desc = unsafe { addr_of_mut!(#desc_ident).as_mut().unwrap_unchecked() };
            unittest::add_test(desc);
            0
        }

        #[used]
        #[cfg_attr(target_os = "linux", link_section = ".init_array")]
        #[cfg_attr(target_os = "none", link_section = ".init_array")]
        #[cfg_attr(target_vendor = "apple", link_section = "__DATA,__mod_init_func")]
        #[cfg_attr(windows, link_section = ".CRT$XCU")]
        static #ctor_ident: extern "C" fn() -> usize = #ctor_fn_ident;

        #item
    }
    .into()
}
