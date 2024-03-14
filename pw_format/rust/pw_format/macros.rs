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

//! The `macro` module provides helpers that simplify writing proc macros
//! that take format strings and arguments.  This is accomplish with three
//! main constructs:
//! * [`FormatAndArgs`]: A struct that implements [syn::parse::Parse] to
//!   parse a format string and its following arguments.
//! * [`FormatMacroGenerator`]: A trait used to implement the macro specific
//!   logic to generate code.
//! * [`generate`]: A function to handle the execution of the proc macro by
//!   calling into a [FormatMacroGenerator].
//!
//! Additionally [`PrintfFormatMacroGenerator`] trait and [`generate_printf`]
//! function are provided to help when implementing generators that need to
//! produce `printf` style format strings as part of their code generation.
//!
//! ## Example
//!
//! An example of implementing a proc macro is provided in the
//! [pw_format_example_macro crate](https://pigweed.googlesource.com/pigweed/pigweed/+/refs/heads/main/pw_format/rust/pw_format_example_macro.rs)
//!
//!

use std::collections::VecDeque;

use proc_macro2::Ident;
use quote::{format_ident, quote, ToTokens};
use syn::{
    parse::{discouraged::Speculative, Parse, ParseStream},
    punctuated::Punctuated,
    spanned::Spanned,
    Expr, ExprCast, LitStr, Token,
};

use crate::{
    ConversionSpec, FormatFragment, FormatString, Length, MinFieldWidth, Precision, Specifier,
};

mod keywords {
    syn::custom_keyword!(PW_FMT_CONCAT);
}

type TokenStream2 = proc_macro2::TokenStream;

/// An error occurring during proc macro evaluation.
///
/// In order to stay as flexible as possible to implementors of
/// [`FormatMacroGenerator`], the error is simply represent by a
/// string.
#[derive(Debug)]
pub struct Error {
    text: String,
}

impl Error {
    /// Create a new proc macro evaluation error.
    pub fn new(text: &str) -> Self {
        Self {
            text: text.to_string(),
        }
    }
}

/// An alias for a Result with an ``Error``
pub type Result<T> = core::result::Result<T, Error>;

/// Style in which to display the an integer.
///
/// In order to maintain compatibility with `printf` style systems, sign
/// and base are combined.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum IntegerDisplayType {
    /// Signed integer
    Signed,
    /// Unsigned integer
    Unsigned,
    /// Unsigned octal
    Octal,
    /// Unsigned hex with lower case letters
    Hex,
    /// Unsigned hex with upper case letters
    UpperHex,
}

impl TryFrom<crate::Specifier> for IntegerDisplayType {
    type Error = Error;

    fn try_from(value: Specifier) -> Result<Self> {
        match value {
            Specifier::Decimal | Specifier::Integer => Ok(Self::Signed),
            Specifier::Unsigned => Ok(Self::Unsigned),
            Specifier::Octal => Ok(Self::Octal),
            Specifier::Hex => Ok(Self::Hex),
            Specifier::UpperHex => Ok(Self::UpperHex),
            _ => Err(Error::new("No valid IntegerDisplayType for {value:?}.")),
        }
    }
}

/// Implemented for testing through the pw_format_test_macros crate.
impl ToTokens for IntegerDisplayType {
    fn to_tokens(&self, tokens: &mut TokenStream2) {
        let new_tokens = match self {
            IntegerDisplayType::Signed => quote!(pw_format::macros::IntegerDisplayType::Signed),
            IntegerDisplayType::Unsigned => {
                quote!(pw_format::macros::IntegerDisplayType::Unsigned)
            }
            IntegerDisplayType::Octal => quote!(pw_format::macros::IntegerDisplayType::Octal),
            IntegerDisplayType::Hex => quote!(pw_format::macros::IntegerDisplayType::Hex),
            IntegerDisplayType::UpperHex => {
                quote!(pw_format::macros::IntegerDisplayType::UpperHex)
            }
        };
        new_tokens.to_tokens(tokens);
    }
}

/// A code generator for implementing a `pw_format` style macro.
///
/// This trait serves as the primary interface between `pw_format` and a
/// proc macro using it to implement format string and argument parsing.  When
/// evaluating the proc macro and generating code, [`generate`] will make
/// repeated calls to [`string_fragment`](FormatMacroGenerator::string_fragment)
/// and the conversion functions.  These calls will be made in the order they
/// appear in the format string.  After all fragments and conversions are
/// processed, [`generate`] will call
/// [`finalize`](FormatMacroGenerator::finalize).
///
/// For an example of implementing a `FormatMacroGenerator` see the
/// [pw_format_example_macro crate](https://pigweed.googlesource.com/pigweed/pigweed/+/refs/heads/main/pw_format/rust/pw_format_example_macro.rs).
pub trait FormatMacroGenerator {
    /// Called by [`generate`] at the end of code generation.
    ///
    /// Consumes `self` and returns the code to be emitted by the proc macro of
    /// and [`Error`].
    fn finalize(self) -> Result<TokenStream2>;

    /// Process a string fragment.
    ///
    /// A string fragment is a string of characters that appear in a format
    /// string.  This is different than a
    /// [`string_conversion`](FormatMacroGenerator::string_conversion) which is
    /// a string provided through a conversion specifier (i.e. `"%s"`).
    fn string_fragment(&mut self, string: &str) -> Result<()>;

    /// Process an integer conversion.
    fn integer_conversion(
        &mut self,
        display: IntegerDisplayType,
        type_width: u8, // This should probably be an enum
        expression: Arg,
    ) -> Result<()>;

    /// Process a string conversion.
    ///
    /// See [`string_fragment`](FormatMacroGenerator::string_fragment) for a
    /// disambiguation between that function and this one.
    fn string_conversion(&mut self, expression: Arg) -> Result<()>;

    /// Process a character conversion.
    fn char_conversion(&mut self, expression: Arg) -> Result<()>;

    /// Process an untyped conversion.
    fn untyped_conversion(&mut self, _expression: Arg) -> Result<()> {
        Err(Error::new("untyped conversion (%v) not supported"))
    }
}

/// An argument to a `pw_format` backed macro.
///
/// `pw_format` backed macros have special case recognition of type casts
/// (`value as ty`) in order to annotate a type for typeless printing w/o
/// relying on experimental features.  If an argument is given in that form,
/// it will be represented as an [`Arg::ExprCast`] here.  Otherwise it will
/// be an [`Arg::Expr`].
#[derive(Clone, Debug)]
pub enum Arg {
    /// An argument that is an type cast expression.
    ExprCast(ExprCast),
    /// An argument that is an expression.
    Expr(Expr),
}

impl Parse for Arg {
    fn parse(input: ParseStream) -> syn::parse::Result<Self> {
        // Try parsing as an explicit cast first.  This lets the user name a
        // type when type_alias_impl_trait is not enabled.
        let fork = input.fork();
        if let Ok(cast) = fork.parse::<ExprCast>() {
            // Speculative parsing and `advance_to` is discouraged due to error
            // presentation.  However, since `ExprCast` is a subset of `Expr`,
            //  any errors in parsing here will be reported when trying to parse
            //  as an `Expr` below.
            input.advance_to(&fork);
            return Ok(Self::ExprCast(cast));
        }

        // Otherwise prase as an expression.
        input.parse::<Expr>().map(|expr| Self::Expr(expr))
    }
}

impl ToTokens for Arg {
    fn to_tokens(&self, tokens: &mut TokenStream2) {
        match self {
            Self::Expr(expr) => expr.to_tokens(tokens),
            Self::ExprCast(cast) => cast.to_tokens(tokens),
        }
    }
}

/// A parsed format string and it's arguments.
///
/// `FormatAndArgs` implements [`syn::parse::Parse`] and can be used to parse
/// arguments to proc maros that take format strings.  Arguments are parsed
/// according to the pattern: `($format_string:literal, $($args:expr),*)`
///
/// To support uses where format strings need to be built by macros at compile
/// time, the format string can be specified as a set of string literals
/// separated by the custom `PW_FMT_CONCAT` keyword.
#[derive(Debug)]
pub struct FormatAndArgs {
    format_string: LitStr,
    parsed: FormatString,
    args: VecDeque<Arg>,
}

impl Parse for FormatAndArgs {
    fn parse(input: ParseStream) -> syn::parse::Result<Self> {
        let punctuated =
            Punctuated::<LitStr, keywords::PW_FMT_CONCAT>::parse_separated_nonempty(input)?;
        let span = punctuated.span();
        let format_string = LitStr::new(
            &punctuated.into_iter().fold(String::new(), |mut acc, s| {
                acc.push_str(&s.value());
                acc
            }),
            span,
        );

        let args = if input.is_empty() {
            // If there are no more tokens, no arguments were specified.
            VecDeque::new()
        } else {
            // Eat the `,` following the format string.
            input.parse::<Token![,]>()?;

            let punctuated = Punctuated::<Arg, Token![,]>::parse_terminated(input)?;
            punctuated.into_iter().collect()
        };

        let parsed = FormatString::parse(&format_string.value()).map_err(|e| {
            syn::Error::new_spanned(
                format_string.to_token_stream(),
                format!("Error parsing format string {e}"),
            )
        })?;

        Ok(FormatAndArgs {
            format_string,
            parsed,
            args,
        })
    }
}

// Grab the next argument returning a descriptive error if no more args are left.
fn next_arg(spec: &ConversionSpec, args: &mut VecDeque<Arg>) -> Result<Arg> {
    args.pop_front()
        .ok_or_else(|| Error::new(&format!("No argument given for {spec:?}")))
}

// Handle a single format conversion specifier (i.e. `%08x`).  Grabs the
// necessary arguments for the specifier from `args` and generates code
// to marshal the arguments into the buffer declared in `_tokenize_to_buffer`.
// Returns an error if args is too short of if a format specifier is unsupported.
fn handle_conversion(
    generator: &mut dyn FormatMacroGenerator,
    spec: &ConversionSpec,
    args: &mut VecDeque<Arg>,
) -> Result<()> {
    match spec.specifier {
        Specifier::Decimal
        | Specifier::Integer
        | Specifier::Octal
        | Specifier::Unsigned
        | Specifier::Hex
        | Specifier::UpperHex => {
            // TODO: b/281862660 - Support Width::Variable and Precision::Variable.
            if spec.min_field_width == MinFieldWidth::Variable {
                return Err(Error::new(
                    "Variable width '*' integer formats are not supported.",
                ));
            }

            if spec.precision == Precision::Variable {
                return Err(Error::new(
                    "Variable precision '*' integer formats are not supported.",
                ));
            }

            let arg = next_arg(spec, args)?;
            let bits = match spec.length.unwrap_or(Length::Long) {
                Length::Char => 8,
                Length::Short => 16,
                Length::Long => 32,
                Length::LongLong => 64,
                Length::IntMax => 64,
                Length::Size => 32,
                Length::PointerDiff => 32,
                Length::LongDouble => {
                    return Err(Error::new(
                        "Long double length parameter invalid for integer formats",
                    ))
                }
            };

            let display: IntegerDisplayType =
                spec.specifier.clone().try_into().expect(
                    "Specifier is guaranteed to convert display type but enclosing match arm.",
                );
            generator.integer_conversion(display, bits, arg)
        }
        Specifier::String => {
            // TODO: b/281862660 - Support Width::Variable and Precision::Variable.
            if spec.min_field_width == MinFieldWidth::Variable {
                return Err(Error::new(
                    "Variable width '*' string formats are not supported.",
                ));
            }

            if spec.precision == Precision::Variable {
                return Err(Error::new(
                    "Variable precision '*' string formats are not supported.",
                ));
            }

            let arg = next_arg(spec, args)?;
            generator.string_conversion(arg)
        }
        Specifier::Char => {
            let arg = next_arg(spec, args)?;
            generator.char_conversion(arg)
        }

        Specifier::Untyped => {
            let arg = next_arg(spec, args)?;
            generator.untyped_conversion(arg)
        }

        Specifier::Double
        | Specifier::UpperDouble
        | Specifier::Exponential
        | Specifier::UpperExponential
        | Specifier::SmallDouble
        | Specifier::UpperSmallDouble => {
            // TODO: b/281862328 - Support floating point numbers.
            Err(Error::new("Floating point numbers are not supported."))
        }

        // TODO: b/281862333 - Support pointers.
        Specifier::Pointer => Err(Error::new("Pointer types are not supported.")),
    }
}

/// Generate code for a `pw_format` style proc macro.
///
/// `generate` takes a [`FormatMacroGenerator`] and a [`FormatAndArgs`] struct
/// and uses them to produce the code output for a proc macro.
pub fn generate(
    mut generator: impl FormatMacroGenerator,
    format_and_args: FormatAndArgs,
) -> core::result::Result<TokenStream2, syn::Error> {
    let mut args = format_and_args.args;
    let mut errors = Vec::new();

    for fragment in format_and_args.parsed.fragments {
        let result = match fragment {
            FormatFragment::Conversion(spec) => handle_conversion(&mut generator, &spec, &mut args),
            FormatFragment::Literal(string) => generator.string_fragment(&string),
            FormatFragment::Percent => generator.string_fragment("%"),
        };
        if let Err(e) = result {
            errors.push(syn::Error::new_spanned(
                format_and_args.format_string.to_token_stream(),
                e.text,
            ));
        }
    }

    if !errors.is_empty() {
        return Err(errors
            .into_iter()
            .reduce(|mut accumulated_errors, error| {
                accumulated_errors.combine(error);
                accumulated_errors
            })
            .expect("errors should not be empty"));
    }

    generator.finalize().map_err(|e| {
        syn::Error::new_spanned(format_and_args.format_string.to_token_stream(), e.text)
    })
}

/// A specialized generator for proc macros that produce `printf` style format strings.
///
/// For proc macros that need to translate a `pw_format` invocation into a
/// `printf` style format string, `PrintfFormatMacroGenerator` offer a
/// specialized form of [`FormatMacroGenerator`] that builds the format string
/// and provides it as an argument to
/// [`finalize`](PrintfFormatMacroGenerator::finalize).
///
/// In cases where a generator needs to override the conversion specifier it
/// can return it from its appropriate conversion method.  An example of using
/// this would be wanting to pass a Rust string directly to a `printf` call
/// over FFI.  In that case,
/// [`string_conversion`](PrintfFormatMacroGenerator::string_conversion) could
/// return `Ok(Some("%.*s".to_string()))` to allow both the length and string
/// pointer to be passed to `printf`.
pub trait PrintfFormatMacroGenerator {
    /// Called by [`generate_printf`] at the end of code generation.
    ///
    /// Works like [`FormatMacroGenerator::finalize`] with the addition of
    /// being provided a `printf_style` format string.
    fn finalize(
        self,
        format_string_fragments: &[PrintfFormatStringFragment],
    ) -> Result<TokenStream2>;

    /// Process a string fragment.
    ///
    /// **NOTE**: This string may contain unescaped `%` characters.
    /// However, most implementations of this train can simply ignore string
    /// fragments as they will be included (with properly escaped `%`
    /// characters) as part of the format string passed to
    /// [`PrintfFormatMacroGenerator::finalize`].
    ///
    /// See [`FormatMacroGenerator::string_fragment`] for a disambiguation
    /// between a string fragment and string conversion.
    fn string_fragment(&mut self, string: &str) -> Result<()>;

    /// Process an integer conversion.
    ///
    /// May optionally return a printf format string (i.e. "%d") to override the
    /// default.
    fn integer_conversion(&mut self, ty: Ident, expression: Arg) -> Result<Option<String>>;

    /// Process a string conversion.
    ///
    /// May optionally return a printf format string (i.e. "%s") to override the
    /// default.
    ///
    /// See [`FormatMacroGenerator::string_fragment`] for a disambiguation
    /// between a string fragment and string conversion.
    fn string_conversion(&mut self, expression: Arg) -> Result<Option<String>>;

    /// Process a character conversion.
    ///
    /// May optionally return a printf format string (i.e. "%c") to override the
    /// default.
    fn char_conversion(&mut self, expression: Arg) -> Result<Option<String>>;

    /// Process and untyped conversion.
    fn untyped_conversion(&mut self, _expression: Arg) -> Result<()> {
        Err(Error::new("untyped conversion not supported"))
    }
}

/// A fragment of a printf format string.
///
/// Printf format strings are built of multiple fragments.  These fragments can
/// be either a string ([`PrintfFormatStringFragment::String`]) or an expression
/// that evaluates to a `const &str` ([`PrintfFormatStringFragment::Expr`]).
/// These fragments can then be used to create a single `const &str` for use by
/// code generation.
///
/// # Example
/// ```
/// use pw_bytes::concat_static_strs;
/// use pw_format::macros::{PrintfFormatStringFragment, Result};
/// use quote::quote;
///
/// fn handle_fragments(format_string_fragments: &[PrintfFormatStringFragment]) -> Result<()> {
///   let format_string_pieces: Vec<_> = format_string_fragments
///     .iter()
///     .map(|fragment| fragment.as_token_stream("__pw_log_backend_crate"))
///     .collect::<Result<Vec<_>>>()?;
///
///   quote! {
///     let format_string = concat_static_strs!("prefix: ", #(#format_string_pieces),*, "\n");
///   };
///   Ok(())
/// }
/// ```
pub enum PrintfFormatStringFragment {
    /// A fragment that is a string.
    String(String),

    /// An expressions that evaluates to a `const &str`.
    Expr(Arg),
}

impl PrintfFormatStringFragment {
    /// Returns a [`proc_macro2::TokenStream`] representing this fragment.
    pub fn as_token_stream(&self, printf_formatter_trait_location: &str) -> Result<TokenStream2> {
        let crate_name = format_ident!("{}", printf_formatter_trait_location);
        match self {
            Self::String(s) => Ok(quote! {#s}),
            #[cfg(not(feature = "nightly_tait"))]
            Self::Expr(arg) => {
                let Arg::ExprCast(cast) = arg else {
                    return Err(Error::new(&format!(
                      "Expected argument to untyped format (%v) to be a cast expression (e.g. x as i32), but found {}.",
                      arg.to_token_stream()
                    )));
                };
                let ty = &cast.ty;
                Ok(quote! {
                  {
                    use #crate_name::PrintfFormatter;
                    <#ty as PrintfFormatter>::FORMAT_ARG
                  }
                })
            }
            #[cfg(feature = "nightly_tait")]
            Self::Expr(expr) => Ok(quote! {
              {
                use #crate_name::PrintfFormatter;
                type T = impl PrintfFormatter;
                let _: &T = &(#expr);
                let arg = <T as PrintfFormatter>::FORMAT_ARG;
                arg
              }
            }),
        }
    }
}

// Wraps a `PrintfFormatMacroGenerator` in a `FormatMacroGenerator` that
// generates the format string as it goes.
struct PrintfGenerator<GENERATOR: PrintfFormatMacroGenerator> {
    inner: GENERATOR,
    format_string_fragments: Vec<PrintfFormatStringFragment>,
}

impl<GENERATOR: PrintfFormatMacroGenerator> PrintfGenerator<GENERATOR> {
    // Append `format_string` to the current set of format string fragments.
    fn append_format_string(&mut self, format_string: &str) {
        // If the last fragment is a string, append to that.
        if let PrintfFormatStringFragment::String(s) = self
            .format_string_fragments
            .last_mut()
            .expect("format_string_fragments always has at least one entry")
        {
            s.push_str(format_string)
        } else {
            // If the last fragment is not a string, add a new string fragment.
            self.format_string_fragments
                .push(PrintfFormatStringFragment::String(
                    format_string.to_string(),
                ));
        }
    }
}

impl<GENERATOR: PrintfFormatMacroGenerator> FormatMacroGenerator for PrintfGenerator<GENERATOR> {
    fn finalize(self) -> Result<TokenStream2> {
        self.inner.finalize(&self.format_string_fragments)
    }

    fn string_fragment(&mut self, string: &str) -> Result<()> {
        // Escape '%' characters.
        let format_string = string.replace("%", "%%");

        self.append_format_string(&format_string);
        self.inner.string_fragment(string)
    }

    fn integer_conversion(
        &mut self,
        display: IntegerDisplayType,
        type_width: u8, // in bits
        expression: Arg,
    ) -> Result<()> {
        let length_modifer = match type_width {
            8 => "hh",
            16 => "h",
            32 => "",
            64 => "ll",
            _ => {
                return Err(Error::new(&format!(
                    "printf backend does not support {} bit field width",
                    type_width
                )))
            }
        };

        let (conversion, ty) = match display {
            IntegerDisplayType::Signed => ("d", format_ident!("i{type_width}")),
            IntegerDisplayType::Unsigned => ("u", format_ident!("u{type_width}")),
            IntegerDisplayType::Octal => ("o", format_ident!("u{type_width}")),
            IntegerDisplayType::Hex => ("x", format_ident!("u{type_width}")),
            IntegerDisplayType::UpperHex => ("X", format_ident!("u{type_width}")),
        };

        match self.inner.integer_conversion(ty, expression)? {
            Some(s) => self.append_format_string(&s),
            None => self.append_format_string(&format!("%{}{}", length_modifer, conversion)),
        }

        Ok(())
    }

    fn string_conversion(&mut self, expression: Arg) -> Result<()> {
        match self.inner.string_conversion(expression)? {
            Some(s) => self.append_format_string(&s),
            None => self.append_format_string("%s"),
        }
        Ok(())
    }

    fn char_conversion(&mut self, expression: Arg) -> Result<()> {
        match self.inner.char_conversion(expression)? {
            Some(s) => self.append_format_string(&s),
            None => self.append_format_string("%c"),
        }
        Ok(())
    }

    fn untyped_conversion(&mut self, expression: Arg) -> Result<()> {
        self.inner.untyped_conversion(expression.clone())?;
        self.append_format_string("%");
        self.format_string_fragments
            .push(PrintfFormatStringFragment::Expr(expression));
        Ok(())
    }
}

/// Generate code for a `pw_format` style proc macro that needs a `printf` format string.
///
/// `generate_printf` is a specialized version of [`generate`] which works with
/// [`PrintfFormatMacroGenerator`]
pub fn generate_printf(
    generator: impl PrintfFormatMacroGenerator,
    format_and_args: FormatAndArgs,
) -> core::result::Result<TokenStream2, syn::Error> {
    let generator = PrintfGenerator {
        inner: generator,
        format_string_fragments: vec![PrintfFormatStringFragment::String("".into())],
    };
    generate(generator, format_and_args)
}

/// A specialized generator for proc macros that produce [`core::fmt`] style format strings.
///
/// For proc macros that need to translate a `pw_format` invocation into a
/// [`core::fmt`] style format string, `CoreFmtFormatMacroGenerator` offer a
/// specialized form of [`FormatMacroGenerator`] that builds the format string
/// and provides it as an argument to
/// [`finalize`](CoreFmtFormatMacroGenerator::finalize).
///
/// In cases where a generator needs to override the conversion specifier (i.e.
/// `{}`, it can return it from its appropriate conversion method.
pub trait CoreFmtFormatMacroGenerator {
    /// Called by [`generate_core_fmt`] at the end of code generation.
    ///
    /// Works like [`FormatMacroGenerator::finalize`] with the addition of
    /// being provided a [`core::fmt`] format string.
    fn finalize(self, format_string: String) -> Result<TokenStream2>;

    /// Process a string fragment.
    ///
    /// **NOTE**: This string may contain unescaped `{` and `}` characters.
    /// However, most implementations of this train can simply ignore string
    /// fragments as they will be included (with properly escaped `{` and `}`
    /// characters) as part of the format string passed to
    /// [`CoreFmtFormatMacroGenerator::finalize`].
    ///
    ///
    /// See [`FormatMacroGenerator::string_fragment`] for a disambiguation
    /// between a string fragment and string conversion.
    fn string_fragment(&mut self, string: &str) -> Result<()>;

    /// Process an integer conversion.
    fn integer_conversion(&mut self, ty: Ident, expression: Arg) -> Result<Option<String>>;

    /// Process a string conversion.
    fn string_conversion(&mut self, expression: Arg) -> Result<Option<String>>;

    /// Process a character conversion.
    fn char_conversion(&mut self, expression: Arg) -> Result<Option<String>>;

    /// Process an untyped conversion.
    fn untyped_conversion(&mut self, _expression: Arg) -> Result<()> {
        Err(Error::new("untyped conversion (%v) not supported"))
    }
}

// Wraps a `CoreFmtFormatMacroGenerator` in a `FormatMacroGenerator` that
// generates the format string as it goes.
struct CoreFmtGenerator<GENERATOR: CoreFmtFormatMacroGenerator> {
    inner: GENERATOR,
    format_string: String,
}

impl<GENERATOR: CoreFmtFormatMacroGenerator> FormatMacroGenerator for CoreFmtGenerator<GENERATOR> {
    fn finalize(self) -> Result<TokenStream2> {
        self.inner.finalize(self.format_string)
    }

    fn string_fragment(&mut self, string: &str) -> Result<()> {
        // Escape '{' and '} characters.
        let format_string = string.replace("{", "{{").replace("}", "}}");

        self.format_string.push_str(&format_string);
        self.inner.string_fragment(string)
    }

    fn integer_conversion(
        &mut self,
        display: IntegerDisplayType,
        type_width: u8, // in bits
        expression: Arg,
    ) -> Result<()> {
        let (conversion, ty) = match display {
            IntegerDisplayType::Signed => ("{}", format_ident!("i{type_width}")),
            IntegerDisplayType::Unsigned => ("{}", format_ident!("u{type_width}")),
            IntegerDisplayType::Octal => ("{:o}", format_ident!("u{type_width}")),
            IntegerDisplayType::Hex => ("{:x}", format_ident!("u{type_width}")),
            IntegerDisplayType::UpperHex => ("{:X}", format_ident!("u{type_width}")),
        };

        match self.inner.integer_conversion(ty, expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str(conversion),
        }

        Ok(())
    }

    fn string_conversion(&mut self, expression: Arg) -> Result<()> {
        match self.inner.string_conversion(expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str("{}"),
        }
        Ok(())
    }

    fn char_conversion(&mut self, expression: Arg) -> Result<()> {
        match self.inner.char_conversion(expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str("{}"),
        }
        Ok(())
    }

    fn untyped_conversion(&mut self, expression: Arg) -> Result<()> {
        self.inner.untyped_conversion(expression)?;
        self.format_string.push_str("{}");
        Ok(())
    }
}

/// Generate code for a `pw_format` style proc macro that needs a [`core::fmt`] format string.
///
/// `generate_core_fmt` is a specialized version of [`generate`] which works with
/// [`CoreFmtFormatMacroGenerator`]
pub fn generate_core_fmt(
    generator: impl CoreFmtFormatMacroGenerator,
    format_and_args: FormatAndArgs,
) -> core::result::Result<TokenStream2, syn::Error> {
    let generator = CoreFmtGenerator {
        inner: generator,
        format_string: "".into(),
    };
    generate(generator, format_and_args)
}
