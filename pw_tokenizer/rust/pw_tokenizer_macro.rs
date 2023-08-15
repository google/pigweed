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

use std::collections::VecDeque;
use std::ffi::CString;

use proc_macro::TokenStream;
use quote::{format_ident, quote, ToTokens};
use syn::{
    parse::{Parse, ParseStream},
    parse_macro_input,
    punctuated::Punctuated,
    Expr, LitStr, Token,
};

use pw_tokenizer_core::{hash_string, TOKENIZER_ENTRY_MAGIC};
use pw_tokenizer_printf as printf;

type TokenStream2 = proc_macro2::TokenStream;

struct Error {
    text: String,
}

impl Error {
    fn new(text: &str) -> Self {
        Self {
            text: text.to_string(),
        }
    }
}

type Result<T> = core::result::Result<T, Error>;

// Handles tokenizing (hashing) `string` and adding it to the token database
// with the specified `domain`.  A detailed description of what's happening is
// found in the docs for [`pw_tokenizer::token`] macro.
fn token_backend(domain: &str, string: &str) -> TokenStream2 {
    let hash = hash_string(string);

    // Line number is omitted as getting that info requires an experimental API:
    // https://doc.rust-lang.org/proc_macro/struct.Span.html#method.start
    let ident = format_ident!("_pw_tokenizer_string_entry_{:08X}", hash);

    // pw_tokenizer is intended for use with ELF files only. Mach-O files (macOS
    // executables) do not support section names longer than 16 characters, so a
    // short, unused section name is used on macOS.
    let section = if cfg!(target_os = "macos") {
        ",pw,".to_string()
    } else {
        format!(".pw_tokenizer.entries.{:08X}", hash)
    };

    let string = CString::new(string).unwrap();
    let string_bytes = string.as_bytes_with_nul();
    let string_bytes_len = string_bytes.len();

    let domain = CString::new(domain).unwrap();
    let domain_bytes = domain.as_bytes_with_nul();
    let domain_bytes_len = domain_bytes.len();

    quote! {
        // Use an inner scope to avoid identifier collision.  Name mangling
        // will disambiguate these in the symbol table.
        {
            #[repr(C, packed(1))]
            struct TokenEntry {
                magic: u32,
                token: u32,
                domain_size: u32,
                string_length: u32,
                domain: [u8; #domain_bytes_len],
                string: [u8; #string_bytes_len],
            };
            // This is currently manually verified to be correct.
            // TODO: b/287132907 - Add integration tests for token database.
            #[link_section = #section ]
            static #ident: TokenEntry = TokenEntry {
                magic: #TOKENIZER_ENTRY_MAGIC,
                token: #hash,
                domain_size: #domain_bytes_len as u32,
                string_length: #string_bytes_len as u32,
                domain: [ #(#domain_bytes),* ],
                string: [ #(#string_bytes),* ],
            };

            #hash
        }
    }
}

// Documented in `pw_tokenizer::token`.
#[proc_macro]
pub fn _token(tokens: TokenStream) -> TokenStream {
    let input = parse_macro_input!(tokens as LitStr);
    token_backend("", &input.value()).into()
}

// Args to tokenize to buffer that are parsed according to the pattern:
//   ($buffer:expr, $format_string:literal, $($args:expr),*)
#[derive(Debug)]
struct TokenizeToBuffer {
    buffer: Expr,
    format_string: LitStr,
    args: VecDeque<Expr>,
}

impl Parse for TokenizeToBuffer {
    fn parse(input: ParseStream) -> syn::parse::Result<Self> {
        let buffer: Expr = input.parse()?;
        input.parse::<Token![,]>()?;
        let format_string: LitStr = input.parse()?;

        let args = if input.is_empty() {
            // If there are no more tokens, no arguments were specified.
            VecDeque::new()
        } else {
            // Eat the `,` following the format string.
            input.parse::<Token![,]>()?;

            let punctuated = Punctuated::<Expr, Token![,]>::parse_terminated(input)?;
            punctuated.into_iter().collect()
        };

        Ok(TokenizeToBuffer {
            buffer,
            format_string,
            args,
        })
    }
}

// Grab the next argument returning a descriptive error if no more args are left.
fn next_arg(spec: &printf::ConversionSpec, args: &mut VecDeque<Expr>) -> Result<Expr> {
    args.pop_front()
        .ok_or_else(|| Error::new(&format!("No argument given for {spec:?}")))
}

// Handle a single format conversion specifier (i.e. `%08x`).  Grabs the
// necessary arguments for the specifier from `args` and generates code
// to marshal the arguments into the buffer declared in `_tokenize_to_buffer`.
// Returns an error if args is too short of if a format specifier is unsupported.
fn handle_conversion(
    spec: &printf::ConversionSpec,
    args: &mut VecDeque<Expr>,
) -> Result<TokenStream2> {
    match spec.specifier {
        printf::Specifier::Decimal
        | printf::Specifier::Integer
        | printf::Specifier::Octal
        | printf::Specifier::Unsigned
        | printf::Specifier::Hex
        | printf::Specifier::UpperHex => {
            // TODO: b/281862660 - Support Width::Variable and Precision::Variable.
            if spec.min_field_width == printf::MinFieldWidth::Variable {
                return Err(Error::new(
                    "Variable width '*' integer formats are not supported.",
                ));
            }

            if spec.precision == printf::Precision::Variable {
                return Err(Error::new(
                    "Variable precision '*' integer formats are not supported.",
                ));
            }

            let arg = next_arg(spec, args)?;
            let bits = match spec.length.unwrap_or(printf::Length::Long) {
                printf::Length::Char => 8,
                printf::Length::Short => 16,
                printf::Length::Long => 32,
                printf::Length::LongLong => 64,
                printf::Length::IntMax => 64,
                printf::Length::Size => 32,
                printf::Length::PointerDiff => 32,
                printf::Length::LongDouble => {
                    return Err(Error::new(
                        "Long double length parameter invalid for integer formats",
                    ))
                }
            };
            let ty = format_ident!("i{bits}");
            Ok(quote! {
              // pw_tokenizer always uses signed packing for all integers.
              cursor.write_signed_varint(#ty::from(#arg) as i64)?;
            })
        }
        printf::Specifier::String => {
            // TODO: b/281862660 - Support Width::Variable and Precision::Variable.
            if spec.min_field_width == printf::MinFieldWidth::Variable {
                return Err(Error::new(
                    "Variable width '*' string formats are not supported.",
                ));
            }

            if spec.precision == printf::Precision::Variable {
                return Err(Error::new(
                    "Variable precision '*' string formats are not supported.",
                ));
            }

            let arg = next_arg(spec, args)?;
            Ok(quote! {
              let mut buffer = __pw_tokenizer_crate::internal::encode_string(&mut cursor, #arg)?;
            })
        }
        printf::Specifier::Char => {
            let arg = next_arg(spec, args)?;
            Ok(quote! {
              cursor.write_u8_le(&u8::from(#arg))?;
            })
        }

        printf::Specifier::Double
        | printf::Specifier::UpperDouble
        | printf::Specifier::Exponential
        | printf::Specifier::UpperExponential
        | printf::Specifier::SmallDouble
        | printf::Specifier::UpperSmallDouble => {
            // TODO: b/281862328 - Support floating point numbers.
            Err(Error::new("Floating point numbers are not supported."))
        }

        // TODO: b/281862333 - Support pointers.
        printf::Specifier::Pointer => Err(Error::new("Pointer types are not supported.")),
    }
}

// Generates code to marshal a tokenized string and arguments into a buffer.
// See [`pw_tokenizer::tokenize_to_buffer`] for details on behavior.
//
// Internally the [`AsMut<u8>`] is wrapped in a [`pw_stream::Cursor`] to
// fill the buffer incrementally.
#[proc_macro]
pub fn _tokenize_to_buffer(tokens: TokenStream) -> TokenStream {
    let input = parse_macro_input!(tokens as TokenizeToBuffer);
    let token = token_backend("", &input.format_string.value());
    let buffer = input.buffer;

    let format_string = input.format_string.value();

    let format = match printf::FormatString::parse(&format_string) {
        Ok(format) => format,
        Err(e) => {
            return syn::Error::new_spanned(
                input.format_string.to_token_stream(),
                format!("Error parsing format string {e}"),
            )
            .to_compile_error()
            .into()
        }
    };
    let mut args = input.args;
    let mut arg_encodings = Vec::new();

    let mut errors = Vec::new();

    for fragment in format.fragments {
        if let printf::FormatFragment::Conversion(spec) = fragment {
            match handle_conversion(&spec, &mut args) {
                Ok(encoding) => arg_encodings.push(encoding),
                Err(e) => errors.push(syn::Error::new_spanned(
                    input.format_string.to_token_stream(),
                    e.text,
                )),
            }
        }
    }

    if !errors.is_empty() {
        return errors
            .into_iter()
            .reduce(|mut accumulated_errors, error| {
                accumulated_errors.combine(error);
                accumulated_errors
            })
            .expect("errors should not be empty")
            .to_compile_error()
            .into();
    }

    let code = quote! {
      {
        // Wrapping code in an internal function to allow `?` to work in
        // functions that don't return Results.
        fn _pw_tokenizer_internal_encode(buffer: &mut [u8], token: u32) -> pw_status::Result<usize> {
          // use pw_tokenizer's private re-export of these pw_stream bits to
          // allow referencing with needing `pw_stream` in scope.
          use __pw_tokenizer_crate::{Cursor, Seek, WriteInteger, WriteVarint};
          let mut cursor = Cursor::new(buffer);
          cursor.write_u32_le(&token)?;
          #(#arg_encodings);*;
          Ok(cursor.stream_position()? as usize)
        }
        _pw_tokenizer_internal_encode(#buffer, #token)
      }
    };
    code.into()
}

// Macros tested in `pw_tokenizer` crate.
#[cfg(test)]
mod tests {}
