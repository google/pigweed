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

//! The `pw_tokenizer_printf` crate is a parser used by `pw_tokenizer`'s
//! proc macros to:
//! * Understand tokenization argument types at compile time.
//! * Syntax check format strings.
//!
//! `pw_tokenizer_printf` is written against `std` and is not intended to be
//! used in an embedded context.  Some efficiency and memory is traded for a
//! more expressive interface that exposes the format string's "syntax tree"
//! to the API client.
//!
//! # Example
//!
//! ```
//! use pw_format::{
//!     ConversionSpec, Flag, FormatFragment, FormatString, Length, Precision, Specifier, MinFieldWidth,
//! };
//!
//! let format_string =
//!   FormatString::parse("long double %+ 4.2Lg is %-03hd%%.").unwrap();
//!
//! assert_eq!(format_string, FormatString {
//!   fragments: vec![
//!       FormatFragment::Literal("long double "),
//!       FormatFragment::Conversion(ConversionSpec {
//!           flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
//!           min_field_width: MinFieldWidth::Fixed(4),
//!           precision: Precision::Fixed(2),
//!           length: Some(Length::LongDouble),
//!           specifier: Specifier::SmallDouble
//!       }),
//!       FormatFragment::Literal(" is "),
//!       FormatFragment::Conversion(ConversionSpec {
//!           flags: [Flag::LeftJustify, Flag::LeadingZeros]
//!               .into_iter()
//!               .collect(),
//!           min_field_width: MinFieldWidth::Fixed(3),
//!           precision: Precision::None,
//!           length: Some(Length::Short),
//!           specifier: Specifier::Decimal
//!       }),
//!       FormatFragment::Percent,
//!       FormatFragment::Literal("."),
//!   ]
//! });
//! ```
#![deny(missing_docs)]

use std::collections::HashSet;

use nom::{
    branch::alt,
    bytes::complete::tag,
    bytes::complete::take_till1,
    character::complete::{anychar, digit1},
    combinator::{map, map_res},
    multi::many0,
    IResult,
};

#[derive(Debug, Clone, PartialEq, Eq)]
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
}

impl TryFrom<char> for Specifier {
    type Error = String;

    fn try_from(value: char) -> Result<Self, Self::Error> {
        match value {
            'd' => Ok(Self::Decimal),
            'i' => Ok(Self::Integer),
            'o' => Ok(Self::Octal),
            'u' => Ok(Self::Unsigned),
            'x' => Ok(Self::Hex),
            'X' => Ok(Self::UpperHex),
            'f' => Ok(Self::Double),
            'F' => Ok(Self::UpperDouble),
            'e' => Ok(Self::Exponential),
            'E' => Ok(Self::UpperExponential),
            'g' => Ok(Self::SmallDouble),
            'G' => Ok(Self::UpperSmallDouble),
            'c' => Ok(Self::Char),
            's' => Ok(Self::String),
            'p' => Ok(Self::Pointer),
            _ => Err(format!("Unsupported format specifier '{}'", value)),
        }
    }
}

#[derive(Debug, Hash, PartialEq, Eq)]
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

impl TryFrom<char> for Flag {
    type Error = String;

    fn try_from(value: char) -> Result<Self, Self::Error> {
        match value {
            '-' => Ok(Self::LeftJustify),
            '+' => Ok(Self::ForceSign),
            ' ' => Ok(Self::SpaceSign),
            '#' => Ok(Self::AlternateSyntax),
            '0' => Ok(Self::LeadingZeros),
            _ => Err(format!("Unsupported flag '{}'", value)),
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
/// A printf minimum field width (the 5 in %5d).
pub enum MinFieldWidth {
    /// No field width specified.
    None,

    /// Fixed field with.
    Fixed(u32),

    /// Variable field width passed as an argument (i.e. %*d).
    Variable,
}

#[derive(Debug, PartialEq, Eq)]
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

#[derive(Debug, PartialEq, Eq)]
/// A printf conversion specification aka a % clause.
pub struct ConversionSpec {
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

#[derive(Debug, PartialEq, Eq)]
/// A fragment of a printf format string.
pub enum FormatFragment<'a> {
    /// A literal string value.
    Literal(&'a str),

    /// A conversion specification (i.e. %d).
    Conversion(ConversionSpec),

    /// An escaped %.
    Percent,
}

#[derive(Debug, PartialEq, Eq)]
/// A parsed printf format string.
pub struct FormatString<'a> {
    /// The [FormatFragment]s that comprise the [FormatString].
    pub fragments: Vec<FormatFragment<'a>>,
}

fn specifier(input: &str) -> IResult<&str, Specifier> {
    map_res(anychar, Specifier::try_from)(input)
}

fn flags(input: &str) -> IResult<&str, HashSet<Flag>> {
    let (input, flags) = many0(map_res(anychar, Flag::try_from))(input)?;

    Ok((input, flags.into_iter().collect()))
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

fn length(input: &str) -> IResult<&str, Option<Length>> {
    alt((
        map(tag("hh"), |_| Some(Length::Char)),
        map(tag("h"), |_| Some(Length::Short)),
        map(tag("ll"), |_| Some(Length::LongLong)), // ll must precede l
        map(tag("l"), |_| Some(Length::Long)),
        map(tag("L"), |_| Some(Length::LongDouble)),
        map(tag("j"), |_| Some(Length::IntMax)),
        map(tag("z"), |_| Some(Length::Size)),
        map(tag("t"), |_| Some(Length::PointerDiff)),
        map(tag(""), |_| None),
    ))(input)
}

fn conversion_spec(input: &str) -> IResult<&str, ConversionSpec> {
    let (input, _) = tag("%")(input)?;
    let (input, flags) = flags(input)?;
    let (input, width) = width(input)?;
    let (input, precision) = precision(input)?;
    let (input, length) = length(input)?;
    let (input, specifier) = specifier(input)?;

    Ok((
        input,
        ConversionSpec {
            flags,
            min_field_width: width,
            precision,
            length,
            specifier,
        },
    ))
}

fn literal_fragment(input: &str) -> IResult<&str, FormatFragment> {
    map(take_till1(|c| c == '%'), FormatFragment::Literal)(input)
}

fn percent_fragment(input: &str) -> IResult<&str, FormatFragment> {
    map(tag("%%"), |_| FormatFragment::Percent)(input)
}

fn conversion_fragment(input: &str) -> IResult<&str, FormatFragment> {
    map(conversion_spec, FormatFragment::Conversion)(input)
}

fn fragment(input: &str) -> IResult<&str, FormatFragment> {
    alt((percent_fragment, conversion_fragment, literal_fragment))(input)
}

fn format_string(input: &str) -> IResult<&str, FormatString> {
    let (input, fragments) = many0(fragment)(input)?;

    Ok((input, FormatString { fragments }))
}

impl<'a> FormatString<'a> {
    /// Parses a printf style format string.
    pub fn parse(s: &'a str) -> Result<Self, String> {
        // TODO: b/281858500 - Add better errors to failed parses.
        let (rest, result) =
            format_string(s).map_err(|e| format!("Failed to parse format string \"{s}\": {e}"))?;

        // If the parser did not consume all the input, return an error.
        if !rest.is_empty() {
            return Err(format!(
                "Failed to parse format string fragment: \"{rest}\""
            ));
        }

        Ok(result)
    }
}

#[cfg(test)]
mod tests;
