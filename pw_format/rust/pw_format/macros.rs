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
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    Expr, LitStr, Token,
};

use crate::{
    ConversionSpec, FormatFragment, FormatString, Length, MinFieldWidth, Precision, Specifier,
};

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
        expression: Expr,
    ) -> Result<()>;

    /// Process a string conversion.
    ///
    /// See [`string_fragment`](FormatMacroGenerator::string_fragment) for a
    /// disambiguation between that function and this one.
    fn string_conversion(&mut self, expression: Expr) -> Result<()>;

    /// Process a character conversion.
    fn char_conversion(&mut self, expression: Expr) -> Result<()>;
}

/// A parsed format string and it's arguments.
///
/// `FormatAndArgs` implements [`syn::parse::Parse`] and can be used to parse
/// arguments to proc maros that take format strings.  Arguments are parsed
/// according to the pattern: `($format_string:literal, $($args:expr),*)`
#[derive(Debug)]
pub struct FormatAndArgs {
    format_string: LitStr,
    parsed: FormatString,
    args: VecDeque<Expr>,
}

impl Parse for FormatAndArgs {
    fn parse(input: ParseStream) -> syn::parse::Result<Self> {
        let format_string = input.parse::<LitStr>()?;

        let args = if input.is_empty() {
            // If there are no more tokens, no arguments were specified.
            VecDeque::new()
        } else {
            // Eat the `,` following the format string.
            input.parse::<Token![,]>()?;

            let punctuated = Punctuated::<Expr, Token![,]>::parse_terminated(input)?;
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
fn next_arg(spec: &ConversionSpec, args: &mut VecDeque<Expr>) -> Result<Expr> {
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
    args: &mut VecDeque<Expr>,
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
    fn finalize(self, format_string: String) -> Result<TokenStream2>;

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
    fn integer_conversion(&mut self, ty: Ident, expression: Expr) -> Result<Option<String>>;

    /// Process a string conversion.
    ///
    /// May optionally return a printf format string (i.e. "%s") to override the
    /// default.
    ///
    /// See [`FormatMacroGenerator::string_fragment`] for a disambiguation
    /// between a string fragment and string conversion.
    /// FIXME: docs
    fn string_conversion(&mut self, expression: Expr) -> Result<Option<String>>;

    /// Process a character conversion.
    ///
    /// May optionally return a printf format string (i.e. "%c") to override the
    /// default.
    fn char_conversion(&mut self, expression: Expr) -> Result<Option<String>>;
}

// Wraps a `PrintfFormatMacroGenerator` in a `FormatMacroGenerator` that
// generates the format string as it goes.
struct PrintfGenerator<GENERATOR: PrintfFormatMacroGenerator> {
    inner: GENERATOR,
    format_string: String,
}

impl<GENERATOR: PrintfFormatMacroGenerator> FormatMacroGenerator for PrintfGenerator<GENERATOR> {
    fn finalize(self) -> Result<TokenStream2> {
        self.inner.finalize(self.format_string)
    }

    fn string_fragment(&mut self, string: &str) -> Result<()> {
        // Escape '%' characters.
        let format_string = string.replace("%", "%%");

        self.format_string.push_str(&format_string);
        self.inner.string_fragment(string)
    }

    fn integer_conversion(
        &mut self,
        display: IntegerDisplayType,
        type_width: u8, // in bits
        expression: Expr,
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
            Some(s) => self.format_string.push_str(&s),
            None => self
                .format_string
                .push_str(&format!("%{}{}", length_modifer, conversion)),
        }

        Ok(())
    }

    fn string_conversion(&mut self, expression: Expr) -> Result<()> {
        match self.inner.string_conversion(expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str("%s"),
        }
        Ok(())
    }

    fn char_conversion(&mut self, expression: Expr) -> Result<()> {
        match self.inner.char_conversion(expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str("%c"),
        }
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
        format_string: "".into(),
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
    fn integer_conversion(&mut self, ty: Ident, expression: Expr) -> Result<Option<String>>;

    /// Process a string conversion.
    fn string_conversion(&mut self, expression: Expr) -> Result<Option<String>>;

    /// Process a character conversion.
    fn char_conversion(&mut self, expression: Expr) -> Result<Option<String>>;
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
        expression: Expr,
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

    fn string_conversion(&mut self, expression: Expr) -> Result<()> {
        match self.inner.string_conversion(expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str("{}"),
        }
        Ok(())
    }

    fn char_conversion(&mut self, expression: Expr) -> Result<()> {
        match self.inner.char_conversion(expression)? {
            Some(s) => self.format_string.push_str(&s),
            None => self.format_string.push_str("{}"),
        }
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
