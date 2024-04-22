// Copyright 2023 The Pigweed Authors
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

// This proc macro crate is a private API for the `pw_tokenizer` crate.
#![doc(hidden)]

use std::ffi::CString;

use proc_macro::TokenStream;
use proc_macro2::Ident;
use quote::{format_ident, quote, ToTokens};
use syn::{
    parse::{Parse, ParseStream},
    parse_macro_input, Expr, LitStr, Token, Type,
};

use pw_format::macros::{
    generate_printf, Arg, FormatAndArgsFlavor, PrintfFormatMacroGenerator,
    PrintfFormatStringFragment, PrintfFormatStringParser, Result,
};
use pw_tokenizer_core::TOKENIZER_ENTRY_MAGIC;

type TokenStream2 = proc_macro2::TokenStream;

// Handles tokenizing (hashing) `fragments` and adding them to the token database
// with the specified `domain`.  A detailed description of what's happening is
// found in the docs for [`pw_tokenizer::token`] macro.
fn token_backend(domain: &str, fragments: &[TokenStream2]) -> TokenStream2 {
    let ident = format_ident!("_PW_TOKENIZER_STRING_ENTRY_RUST");

    // pw_tokenizer is intended for use with ELF files only. Mach-O files (macOS
    // executables) do not support section names longer than 16 characters, so a
    // short, unused section name is used on macOS.
    let section = if cfg!(target_os = "macos") {
        ",pw,".to_string()
    } else {
        format!(".pw_tokenizer.entries.rust")
    };

    let domain = CString::new(domain).unwrap();
    let domain_bytes = domain.as_bytes_with_nul();
    let domain_bytes_len = domain_bytes.len();

    quote! {
        // Use an inner scope to avoid identifier collision.  Name mangling
        // will disambiguate these in the symbol table.
        {
            const STRING: &str = __pw_tokenizer_crate::concat_static_strs!(#(#fragments),*);
            const STRING_BYTES: &[u8] = STRING.as_bytes();
            const STRING_LEN: usize = STRING_BYTES.len();

            const HASH: u32 = __pw_tokenizer_crate::hash_string(STRING);

            #[repr(C, packed(1))]
            struct TokenEntry {
                magic: u32,
                token: u32,
                domain_size: u32,
                string_length: u32,
                domain: [u8; #domain_bytes_len],
                string: [u8; STRING_LEN],
                null_terminator: u8,
            };
            // This is currently manually verified to be correct.
            // TODO: b/287132907 - Add integration tests for token database.
            #[link_section = #section ]
            #[used]
            static #ident: TokenEntry = TokenEntry {
                magic: #TOKENIZER_ENTRY_MAGIC,
                token: HASH,
                domain_size: #domain_bytes_len as u32,
                string_length: (STRING_LEN + 1) as u32,
                domain: [ #(#domain_bytes),* ],
                // Safety: `STRING_LEN` is declared as the length of `STRING_BYTES` above.
                string: unsafe { *::core::mem::transmute::<_, *const [u8; STRING_LEN]>(STRING_BYTES.as_ptr()) },
                null_terminator: 0u8,
            };

            HASH
        }
    }
}

// Documented in `pw_tokenizer::token`.
#[proc_macro]
pub fn _token(tokens: TokenStream) -> TokenStream {
    let input = parse_macro_input!(tokens as LitStr);
    token_backend("", &[input.into_token_stream()]).into()
}

// Args to tokenize to buffer that are parsed according to the pattern:
//   ($buffer:expr, $format_string:literal, $($args:expr),*)
#[derive(Debug)]
struct TokenizeToBufferArgs {
    buffer: Expr,
    format_and_args: FormatAndArgsFlavor<PrintfFormatStringParser>,
}

impl Parse for TokenizeToBufferArgs {
    fn parse(input: ParseStream) -> syn::parse::Result<Self> {
        let buffer: Expr = input.parse()?;
        input.parse::<Token![,]>()?;
        let format_and_args: FormatAndArgsFlavor<_> = input.parse()?;

        Ok(TokenizeToBufferArgs {
            buffer,
            format_and_args,
        })
    }
}

// A PrintfFormatMacroGenerator that provides the code generation backend for
// the `tokenize_to_buffer!` macro.
struct TokenizeToBufferGenerator<'a> {
    domain: &'a str,
    buffer: &'a Expr,
    encoding_fragments: Vec<TokenStream2>,
}

impl<'a> TokenizeToBufferGenerator<'a> {
    fn new(domain: &'a str, buffer: &'a Expr) -> Self {
        Self {
            domain,
            buffer,
            encoding_fragments: Vec::new(),
        }
    }
}

impl<'a> PrintfFormatMacroGenerator for TokenizeToBufferGenerator<'a> {
    fn finalize(
        self,
        format_string_fragments: &[PrintfFormatStringFragment],
    ) -> Result<TokenStream2> {
        // Locally scoped aliases so we can refer to them in `quote!()`
        let buffer = self.buffer;
        let encoding_fragments = self.encoding_fragments;

        let format_string_pieces: Vec<_> = format_string_fragments
            .iter()
            .map(|fragment| fragment.as_token_stream("__pw_tokenizer_crate"))
            .collect::<Result<Vec<_>>>()?;

        // `token_backend` returns a `TokenStream2` which both inserts the
        // string into the token database and returns the hash value.
        let token = token_backend(self.domain, &format_string_pieces);

        if encoding_fragments.is_empty() {
            Ok(quote! {
              {
                __pw_tokenizer_crate::internal::tokenize_to_buffer_no_args(#buffer, #token)
              }
            })
        } else {
            Ok(quote! {
              {
                use __pw_tokenizer_crate::internal::Argument;
                __pw_tokenizer_crate::internal::tokenize_to_buffer(
                  #buffer,
                  #token,
                  &[#(#encoding_fragments),*]
                )
              }
            })
        }
    }

    fn string_fragment(&mut self, _string: &str) -> Result<()> {
        // String fragments are encoded directly into the format string.
        Ok(())
    }

    fn integer_conversion(&mut self, ty: Ident, expression: Arg) -> Result<Option<String>> {
        self.encoding_fragments.push(quote! {
          Argument::Varint(#ty::from(#expression) as i32)
        });

        Ok(None)
    }

    fn string_conversion(&mut self, expression: Arg) -> Result<Option<String>> {
        self.encoding_fragments.push(quote! {
          Argument::String(#expression)
        });
        Ok(None)
    }

    fn char_conversion(&mut self, expression: Arg) -> Result<Option<String>> {
        self.encoding_fragments.push(quote! {
          Argument::Char(u8::from(#expression))
        });
        Ok(None)
    }

    fn untyped_conversion(&mut self, expression: Arg) -> Result<()> {
        self.encoding_fragments.push(quote! {
          __pw_tokenizer_crate::internal::Encoder::encode(#expression, &mut cursor)?;
        });
        Ok(())
    }
}

// Generates code to marshal a tokenized string and arguments into a buffer.
// See [`pw_tokenizer::tokenize_to_buffer`] for details on behavior.
//
// Internally the [`AsMut<u8>`] is wrapped in a [`pw_stream::Cursor`] to
// fill the buffer incrementally.
#[proc_macro]
pub fn _tokenize_to_buffer(tokens: TokenStream) -> TokenStream {
    let input = parse_macro_input!(tokens as TokenizeToBufferArgs);

    // Hard codes domain to "".
    let generator = TokenizeToBufferGenerator::new("", &input.buffer);

    match generate_printf(generator, input.format_and_args.into()) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

// Args to tokenize to buffer that are parsed according to the pattern:
//   ($ty:ty, $format_string:literal, $($args:expr),*)
#[derive(Debug)]
struct TokenizeToWriterArgs {
    ty: Type,
    format_and_args: FormatAndArgsFlavor<PrintfFormatStringParser>,
}

impl Parse for TokenizeToWriterArgs {
    fn parse(input: ParseStream) -> syn::parse::Result<Self> {
        let ty: Type = input.parse()?;
        input.parse::<Token![,]>()?;
        let format_and_args: FormatAndArgsFlavor<_> = input.parse()?;

        Ok(Self {
            ty,
            format_and_args,
        })
    }
}

// A PrintfFormatMacroGenerator that provides the code generation backend for
// the `tokenize_to_writer!` macro.
struct TokenizeToWriterGenerator<'a> {
    domain: &'a str,
    ty: &'a Type,
    encoding_fragments: Vec<TokenStream2>,
}

impl<'a> TokenizeToWriterGenerator<'a> {
    fn new(domain: &'a str, ty: &'a Type) -> Self {
        Self {
            domain,
            ty,
            encoding_fragments: Vec::new(),
        }
    }
}

impl<'a> PrintfFormatMacroGenerator for TokenizeToWriterGenerator<'a> {
    fn finalize(
        self,
        format_string_fragments: &[PrintfFormatStringFragment],
    ) -> Result<TokenStream2> {
        // Locally scoped aliases so we can refer to them in `quote!()`
        let ty = self.ty;
        let encoding_fragments = self.encoding_fragments;

        let format_string_pieces: Vec<_> = format_string_fragments
            .iter()
            .map(|fragment| fragment.as_token_stream("__pw_tokenizer_crate"))
            .collect::<Result<Vec<_>>>()?;

        // `token_backend` returns a `TokenStream2` which both inserts the
        // string into the token database and returns the hash value.
        let token = token_backend(self.domain, &format_string_pieces);

        if encoding_fragments.is_empty() {
            Ok(quote! {
              {
                __pw_tokenizer_crate::internal::tokenize_to_writer_no_args::<#ty>(#token)
              }
            })
        } else {
            Ok(quote! {
              {
                use __pw_tokenizer_crate::internal::Argument;
                __pw_tokenizer_crate::internal::tokenize_to_writer::<#ty>(
                  #token,
                  &[#(#encoding_fragments),*]
                )
              }
            })
        }
    }

    fn string_fragment(&mut self, _string: &str) -> Result<()> {
        // String fragments are encoded directly into the format string.
        Ok(())
    }

    fn integer_conversion(&mut self, ty: Ident, expression: Arg) -> Result<Option<String>> {
        self.encoding_fragments.push(quote! {
          Argument::Varint(#ty::from(#expression) as i32)
        });

        Ok(None)
    }

    fn string_conversion(&mut self, expression: Arg) -> Result<Option<String>> {
        self.encoding_fragments.push(quote! {
          Argument::String(#expression)
        });
        Ok(None)
    }

    fn char_conversion(&mut self, expression: Arg) -> Result<Option<String>> {
        self.encoding_fragments.push(quote! {
          Argument::Char(u8::from(#expression))
        });
        Ok(None)
    }

    fn untyped_conversion(&mut self, expression: Arg) -> Result<()> {
        self.encoding_fragments.push(quote! {
          Argument::from(#expression)
        });
        Ok(())
    }
}

#[proc_macro]
pub fn _tokenize_to_writer(tokens: TokenStream) -> TokenStream {
    let input = parse_macro_input!(tokens as TokenizeToWriterArgs);

    // Hard codes domain to "".
    let generator = TokenizeToWriterGenerator::new("", &input.ty);

    match generate_printf(generator, input.format_and_args.into()) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

// Macros tested in `pw_tokenizer` crate.
#[cfg(test)]
mod tests {}
