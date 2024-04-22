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

//! The `pw_format` crate is a parser used to implement proc macros that:
//! * Understand format string argument types at compile time.
//! * Syntax check format strings.
//!
//! `pw_format` is written against `std` and is not intended to be
//! used in an embedded context.  Some efficiency and memory is traded for a
//! more expressive interface that exposes the format string's "syntax tree"
//! to the API client.
//!
//! # Proc Macros
//!
//! The [`macros`] module provides infrastructure for implementing proc macros
//! that take format strings as arguments.
//!
//! # Example
//!
//! ```
//! use pw_format::{
//!     Alignment, Argument, ConversionSpec, Flag, FormatFragment, FormatString,
//!     Length, Precision, Specifier, MinFieldWidth,
//! };
//!
//! let format_string =
//!   FormatString::parse_printf("long double %+ 4.2Lg is %-03hd%%.").unwrap();
//!
//! assert_eq!(format_string, FormatString {
//!   fragments: vec![
//!       FormatFragment::Literal("long double ".to_string()),
//!       FormatFragment::Conversion(ConversionSpec {
//!           argument: Argument::None,
//!           fill: ' ',
//!           alignment: Alignment::None,
//!           flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
//!           min_field_width: MinFieldWidth::Fixed(4),
//!           precision: Precision::Fixed(2),
//!           length: Some(Length::LongDouble),
//!           specifier: Specifier::SmallDouble
//!       }),
//!       FormatFragment::Literal(" is ".to_string()),
//!       FormatFragment::Conversion(ConversionSpec {
//!           argument: Argument::None,
//!           fill: ' ',
//!           alignment: Alignment::Left,
//!           flags: [Flag::LeftJustify, Flag::LeadingZeros]
//!               .into_iter()
//!               .collect(),
//!           min_field_width: MinFieldWidth::Fixed(3),
//!           precision: Precision::None,
//!           length: Some(Length::Short),
//!           specifier: Specifier::Decimal
//!       }),
//!       FormatFragment::Literal("%.".to_string()),
//!   ]
//! });
//! ```
#![deny(missing_docs)]
//#![feature(type_alias_impl_trait)]

use std::collections::HashSet;

use nom::{
    branch::alt,
    bytes::complete::tag,
    character::complete::digit1,
    combinator::{map, map_res},
    IResult,
};

pub mod macros;

mod core_fmt;
mod printf;

#[derive(Clone, Debug, PartialEq, Eq)]
/// A printf specifier (the 'd' in %d).
pub enum Specifier {
    /// `%d`
    Decimal,

    /// `%i`
    Integer,

    /// `%o`
    Octal,

    /// `%u`
    Unsigned,

    /// `%x`
    Hex,

    /// `%X`
    UpperHex,

    /// `%f`
    Double,

    /// `%F`
    UpperDouble,

    /// `%e`
    Exponential,

    /// `%E`
    UpperExponential,

    /// `%g`
    SmallDouble,

    /// `%G`
    UpperSmallDouble,

    /// `%c`
    Char,

    /// `%s`
    String,

    /// `%p`
    Pointer,

    /// `%v`
    Untyped,

    /// `core::fmt`'s `{:?}`
    Debug,

    /// `core::fmt`'s `{:x?}`
    HexDebug,

    /// `core::fmt`'s `{:X?}`
    UpperHexDebug,

    /// `core::fmt`'s `{:b}`
    Binary,
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
/// A printf flag (the '+' in %+d).
pub enum Flag {
    /// `-`
    LeftJustify,

    /// `+`
    ForceSign,

    /// ` `
    SpaceSign,

    /// `#`
    AlternateSyntax,

    /// `0`
    LeadingZeros,
}

#[derive(Clone, Debug, PartialEq, Eq)]
/// A printf minimum field width (the 5 in %5d).
pub enum MinFieldWidth {
    /// No field width specified.
    None,

    /// Fixed field with.
    Fixed(u32),

    /// Variable field width passed as an argument (i.e. %*d).
    Variable,
}

#[derive(Clone, Debug, PartialEq, Eq)]
/// A printf precision (the .5 in %.5d).
///
/// For string conversions (%s) this is treated as the maximum number of
/// bytes of the string to output.
pub enum Precision {
    /// No precision specified.
    None,

    /// Fixed precision.
    Fixed(u32),

    /// Variable precision passed as an argument (i.e. %.*f).
    Variable,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// A printf length (the l in %ld).
pub enum Length {
    /// `hh`
    Char,

    /// `h`
    Short,

    /// `l`
    Long,

    /// `ll`
    LongLong,

    /// `L`
    LongDouble,

    /// `j`
    IntMax,

    /// `z`
    Size,

    /// `t`
    PointerDiff,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// A core::fmt alignment spec.
pub enum Alignment {
    /// No alignment
    None,

    /// Left alignment (`<`)
    Left,

    /// Center alignment (`^`)
    Center,

    /// Right alignment (`>`)
    Right,
}

#[derive(Clone, Debug, PartialEq, Eq)]
/// An argument in a core::fmt style alignment spec.
///
/// i.e. the var_name in `{var_name:#0x}`
pub enum Argument {
    /// No argument
    None,

    /// A positional argument (i.e. `{0}`).
    Positional(usize),

    /// A named argument (i.e. `{var_name}`).
    Named(String),
}

#[derive(Clone, Debug, PartialEq, Eq)]
/// A printf conversion specification aka a % clause.
pub struct ConversionSpec {
    /// ConversionSpec's argument.
    pub argument: Argument,
    /// ConversionSpec's fill character.
    pub fill: char,
    /// ConversionSpec's field alignment.
    pub alignment: Alignment,
    /// ConversionSpec's set of [Flag]s.
    pub flags: HashSet<Flag>,
    /// ConversionSpec's minimum field width argument.
    pub min_field_width: MinFieldWidth,
    /// ConversionSpec's [Precision] argument.
    pub precision: Precision,
    /// ConversionSpec's [Length] argument.
    pub length: Option<Length>,
    /// ConversionSpec's [Specifier].
    pub specifier: Specifier,
}

#[derive(Clone, Debug, PartialEq, Eq)]
/// A fragment of a printf format string.
pub enum FormatFragment {
    /// A literal string value.
    Literal(String),

    /// A conversion specification (i.e. %d).
    Conversion(ConversionSpec),
}

impl FormatFragment {
    /// Try to append `fragment` to `self`.
    ///
    /// Returns `None` if the appending succeeds and `Some<fragment>` if it fails.
    fn try_append<'a>(&mut self, fragment: &'a FormatFragment) -> Option<&'a FormatFragment> {
        let Self::Literal(literal_fragment) = &fragment else {
            return Some(fragment);
        };

        let Self::Literal(ref mut literal_self) = self else {
            return Some(fragment);
        };

        literal_self.push_str(literal_fragment);

        None
    }
}

#[derive(Debug, PartialEq, Eq)]
/// A parsed printf format string.
pub struct FormatString {
    /// The [FormatFragment]s that comprise the [FormatString].
    pub fragments: Vec<FormatFragment>,
}

impl FormatString {
    /// Parses a printf style format string.
    pub fn parse_printf(s: &str) -> Result<Self, String> {
        // TODO: b/281858500 - Add better errors to failed parses.
        let (rest, result) = printf::format_string(s)
            .map_err(|e| format!("Failed to parse format string \"{s}\": {e}"))?;

        // If the parser did not consume all the input, return an error.
        if !rest.is_empty() {
            return Err(format!(
                "Failed to parse format string fragment: \"{rest}\""
            ));
        }

        Ok(result)
    }

    /// Parses a core::fmt style format string.
    pub fn parse_core_fmt(s: &str) -> Result<Self, String> {
        // TODO: b/281858500 - Add better errors to failed parses.
        let (rest, result) = core_fmt::format_string(s)
            .map_err(|e| format!("Failed to parse format string \"{s}\": {e}"))?;

        // If the parser did not consume all the input, return an error.
        if !rest.is_empty() {
            return Err(format!("Failed to parse format string: \"{rest}\""));
        }

        Ok(result)
    }

    /// Creates a `FormatString` from a slice of fragments.
    ///
    /// This primary responsibility of this function is to merge literal
    /// fragments.  Adjacent literal fragments occur when a parser parses
    /// escape sequences.  Merging them here allows a
    /// [`macros::FormatMacroGenerator`] to not worry about the escape codes.
    pub(crate) fn from_fragments(fragments: &[FormatFragment]) -> Self {
        Self {
            fragments: fragments
                .iter()
                .fold(Vec::<_>::new(), |mut fragments, fragment| {
                    // Collapse adjacent literal fragments.
                    let Some(last) = fragments.last_mut() else {
                        // If there are no accumulated fragments, add this one and return.
                        fragments.push((*fragment).clone());
                        return fragments;
                    };
                    if let Some(fragment) = last.try_append(fragment) {
                        // If the fragments were able to append, no more work to do
                        fragments.push((*fragment).clone());
                    };
                    fragments
                }),
        }
    }
}

fn variable_width(input: &str) -> IResult<&str, MinFieldWidth> {
    map(tag("*"), |_| MinFieldWidth::Variable)(input)
}

fn fixed_width(input: &str) -> IResult<&str, MinFieldWidth> {
    map_res(
        digit1,
        |value: &str| -> Result<MinFieldWidth, std::num::ParseIntError> {
            Ok(MinFieldWidth::Fixed(value.parse()?))
        },
    )(input)
}

fn no_width(input: &str) -> IResult<&str, MinFieldWidth> {
    Ok((input, MinFieldWidth::None))
}

fn width(input: &str) -> IResult<&str, MinFieldWidth> {
    alt((variable_width, fixed_width, no_width))(input)
}

fn variable_precision(input: &str) -> IResult<&str, Precision> {
    let (input, _) = tag(".")(input)?;
    map(tag("*"), |_| Precision::Variable)(input)
}

fn fixed_precision(input: &str) -> IResult<&str, Precision> {
    let (input, _) = tag(".")(input)?;
    map_res(
        digit1,
        |value: &str| -> Result<Precision, std::num::ParseIntError> {
            Ok(Precision::Fixed(value.parse()?))
        },
    )(input)
}

fn no_precision(input: &str) -> IResult<&str, Precision> {
    Ok((input, Precision::None))
}

fn precision(input: &str) -> IResult<&str, Precision> {
    alt((variable_precision, fixed_precision, no_precision))(input)
}

#[cfg(test)]
mod tests;
