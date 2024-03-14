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

use super::*;

#[test]
fn test_specifier() {
    assert_eq!(specifier("d"), Ok(("", Specifier::Decimal)));
    assert_eq!(specifier("i"), Ok(("", Specifier::Integer)));
    assert_eq!(specifier("o"), Ok(("", Specifier::Octal)));
    assert_eq!(specifier("u"), Ok(("", Specifier::Unsigned)));
    assert_eq!(specifier("x"), Ok(("", Specifier::Hex)));
    assert_eq!(specifier("X"), Ok(("", Specifier::UpperHex)));
    assert_eq!(specifier("f"), Ok(("", Specifier::Double)));
    assert_eq!(specifier("F"), Ok(("", Specifier::UpperDouble)));
    assert_eq!(specifier("e"), Ok(("", Specifier::Exponential)));
    assert_eq!(specifier("E"), Ok(("", Specifier::UpperExponential)));
    assert_eq!(specifier("g"), Ok(("", Specifier::SmallDouble)));
    assert_eq!(specifier("G"), Ok(("", Specifier::UpperSmallDouble)));
    assert_eq!(specifier("c"), Ok(("", Specifier::Char)));
    assert_eq!(specifier("s"), Ok(("", Specifier::String)));
    assert_eq!(specifier("p"), Ok(("", Specifier::Pointer)));
    assert_eq!(specifier("v"), Ok(("", Specifier::Untyped)));

    assert_eq!(
        specifier("z"),
        Err(nom::Err::Error(nom::error::Error {
            input: "z",
            code: nom::error::ErrorKind::MapRes
        }))
    );
}

#[test]
fn test_flags() {
    // Parse all the flags
    assert_eq!(
        flags("-+ #0"),
        Ok((
            "",
            vec![
                Flag::LeftJustify,
                Flag::ForceSign,
                Flag::SpaceSign,
                Flag::AlternateSyntax,
                Flag::LeadingZeros,
            ]
            .into_iter()
            .collect()
        ))
    );

    // Parse all the flags but reversed.  Should produce the same set.
    assert_eq!(
        flags("0# +-"),
        Ok((
            "",
            vec![
                Flag::LeftJustify,
                Flag::ForceSign,
                Flag::SpaceSign,
                Flag::AlternateSyntax,
                Flag::LeadingZeros,
            ]
            .into_iter()
            .collect()
        ))
    );

    // Non-flag characters after flags are not parsed.
    assert_eq!(
        flags("0d"),
        Ok(("d", vec![Flag::LeadingZeros,].into_iter().collect()))
    );

    // No flag characters returns empty set.
    assert_eq!(flags("d"), Ok(("d", vec![].into_iter().collect())));
}

#[test]
fn test_width() {
    assert_eq!(
        width("1234567890d"),
        Ok(("d", MinFieldWidth::Fixed(1234567890)))
    );
    assert_eq!(width("*d"), Ok(("d", MinFieldWidth::Variable)));
    assert_eq!(width("d"), Ok(("d", MinFieldWidth::None)));
}

#[test]
fn test_precision() {
    assert_eq!(
        precision(".1234567890f"),
        Ok(("f", Precision::Fixed(1234567890)))
    );
    assert_eq!(precision(".*f"), Ok(("f", Precision::Variable)));
    assert_eq!(precision("f"), Ok(("f", Precision::None)));
}
#[test]
fn test_length() {
    assert_eq!(length("hhd"), Ok(("d", Some(Length::Char))));
    assert_eq!(length("hd"), Ok(("d", Some(Length::Short))));
    assert_eq!(length("ld"), Ok(("d", Some(Length::Long))));
    assert_eq!(length("lld"), Ok(("d", Some(Length::LongLong))));
    assert_eq!(length("Lf"), Ok(("f", Some(Length::LongDouble))));
    assert_eq!(length("jd"), Ok(("d", Some(Length::IntMax))));
    assert_eq!(length("zd"), Ok(("d", Some(Length::Size))));
    assert_eq!(length("td"), Ok(("d", Some(Length::PointerDiff))));
    assert_eq!(length("d"), Ok(("d", None)));
}

#[test]
fn test_conversion_spec() {
    assert_eq!(
        conversion_spec("%d"),
        Ok((
            "",
            ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: None,
                specifier: Specifier::Decimal
            }
        ))
    );

    assert_eq!(
        conversion_spec("%04ld"),
        Ok((
            "",
            ConversionSpec {
                flags: [Flag::LeadingZeros].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(4),
                precision: Precision::None,
                length: Some(Length::Long),
                specifier: Specifier::Decimal
            }
        ))
    );

    assert_eq!(
        conversion_spec("%- 4.2Lg"),
        Ok((
            "",
            ConversionSpec {
                flags: [Flag::LeftJustify, Flag::SpaceSign].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(4),
                precision: Precision::Fixed(2),
                length: Some(Length::LongDouble),
                specifier: Specifier::SmallDouble
            }
        ))
    );
}

#[test]
fn test_format_string() {
    assert_eq!(
        format_string("long double %+ 4.2Lg is %-03hd%%."),
        Ok((
            "",
            FormatString {
                fragments: vec![
                    FormatFragment::Literal("long double ".to_string()),
                    FormatFragment::Conversion(ConversionSpec {
                        flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
                        min_field_width: MinFieldWidth::Fixed(4),
                        precision: Precision::Fixed(2),
                        length: Some(Length::LongDouble),
                        specifier: Specifier::SmallDouble
                    }),
                    FormatFragment::Literal(" is ".to_string()),
                    FormatFragment::Conversion(ConversionSpec {
                        flags: [Flag::LeftJustify, Flag::LeadingZeros]
                            .into_iter()
                            .collect(),
                        min_field_width: MinFieldWidth::Fixed(3),
                        precision: Precision::None,
                        length: Some(Length::Short),
                        specifier: Specifier::Decimal
                    }),
                    FormatFragment::Percent,
                    FormatFragment::Literal(".".to_string()),
                ]
            }
        ))
    );
}

#[test]
fn test_parse() {
    assert_eq!(
        FormatString::parse("long double %+ 4.2Lg is %-03hd%%."),
        Ok(FormatString {
            fragments: vec![
                FormatFragment::Literal("long double ".to_string()),
                FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::Fixed(4),
                    precision: Precision::Fixed(2),
                    length: Some(Length::LongDouble),
                    specifier: Specifier::SmallDouble
                }),
                FormatFragment::Literal(" is ".to_string()),
                FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::LeftJustify, Flag::LeadingZeros]
                        .into_iter()
                        .collect(),
                    min_field_width: MinFieldWidth::Fixed(3),
                    precision: Precision::None,
                    length: Some(Length::Short),
                    specifier: Specifier::Decimal
                }),
                FormatFragment::Percent,
                FormatFragment::Literal(".".to_string()),
            ]
        })
    );
}

//
// The following test cases are from //pw_tokenizer/py/decode_test.py
//

#[test]
fn test_percent() {
    assert_eq!(
        FormatString::parse("%%"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Percent],
        }),
    );
}

#[test]
fn test_percent_with_leading_plus_fails() {
    assert!(FormatString::parse("%+%").is_err());
}

#[test]
fn test_percent_with_leading_negative_fails() {
    assert!(FormatString::parse("%-%").is_err());
}

#[test]
fn test_percent_with_leading_space_fails() {
    assert!(FormatString::parse("% %").is_err());
}

#[test]
fn test_percent_with_leading_hash_fails() {
    assert!(FormatString::parse("%#%").is_err());
}

#[test]
fn test_percent_with_leading_zero_fails() {
    assert!(FormatString::parse("%0%").is_err());
}

#[test]
fn test_percent_with_length_fails() {
    assert!(FormatString::parse("%hh%").is_err());
    assert!(FormatString::parse("%h%").is_err());
    assert!(FormatString::parse("%l%").is_err());
    assert!(FormatString::parse("%L%").is_err());
    assert!(FormatString::parse("%j%").is_err());
    assert!(FormatString::parse("%z%").is_err());
    assert!(FormatString::parse("%t%").is_err());
}

#[test]
fn test_percent_with_width_fails() {
    assert!(FormatString::parse("%9%").is_err());
}

#[test]
fn test_percent_with_multidigit_width_fails() {
    assert!(FormatString::parse("%10%").is_err());
}

#[test]
fn test_percent_with_star_width_fails() {
    assert!(FormatString::parse("%*%").is_err());
}

#[test]
fn test_percent_with_precision_fails() {
    assert!(FormatString::parse("%.5%").is_err());
}

#[test]
fn test_percent_with_multidigit_precision_fails() {
    assert!(FormatString::parse("%.10%").is_err());
}

#[test]
fn test_percent_with_star_precision_fails() {
    assert!(FormatString::parse("%*%").is_err());
}

const INTEGERS: &'static [(&'static str, Specifier)] = &[
    ("d", Specifier::Decimal),
    ("i", Specifier::Integer),
    ("o", Specifier::Octal),
    ("u", Specifier::Unsigned),
    ("x", Specifier::Hex),
    ("X", Specifier::UpperHex),
    // While not strictly an integer pointers take the same args as integers.
    ("p", Specifier::Pointer),
];

#[test]
fn test_integer() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_minus() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%-5{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::LeftJustify].into_iter().collect(),
                    min_field_width: MinFieldWidth::Fixed(5),
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_plus() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%+{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_blank_space() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("% {format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_plus_and_blank_space_ignores_blank_space() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%+ {format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("% +{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_hash() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%#{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::AlternateSyntax].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone(),
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_zero() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%0{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::LeadingZeros].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_integer_with_length() {
    for (format_char, specifier) in INTEGERS {
        assert_eq!(
            FormatString::parse(&format!("%hh{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Char),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%h{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Short),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%l{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Long),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%ll{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::LongLong),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%j{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::IntMax),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%z{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Size),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%t{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::PointerDiff),
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

const FLOATS: &'static [(&'static str, Specifier)] = &[
    ("f", Specifier::Double),
    ("F", Specifier::UpperDouble),
    ("e", Specifier::Exponential),
    ("E", Specifier::UpperExponential),
    ("g", Specifier::SmallDouble),
    ("G", Specifier::UpperSmallDouble),
];

#[test]
fn test_float() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_minus() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%-10{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::LeftJustify].into_iter().collect(),
                    min_field_width: MinFieldWidth::Fixed(10),
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_plus() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%+{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_blank_space() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("% {format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_plus_and_blank_space_ignores_blank_space() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%+ {format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("% +{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::ForceSign, Flag::SpaceSign].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_hash() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%.0{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::Fixed(0),
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%#.0{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::AlternateSyntax].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::Fixed(0),
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_zero() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%010{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [Flag::LeadingZeros].into_iter().collect(),
                    min_field_width: MinFieldWidth::Fixed(10),
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_length() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%hh{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Char),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%h{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Short),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%l{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Long),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%ll{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::LongLong),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%j{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::IntMax),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%z{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::Size),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%t{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::PointerDiff),
                    specifier: specifier.clone()
                })]
            })
        );

        assert_eq!(
            FormatString::parse(&format!("%L{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: HashSet::new(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::None,
                    length: Some(Length::LongDouble),
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_width() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%9{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::Fixed(9),
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_multidigit_width() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%10{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::Fixed(10),
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_star_width() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%*{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::Variable,
                    precision: Precision::None,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_precision() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%.4{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::Fixed(4),
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_multidigit_precision() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%.10{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::Fixed(10),
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_star_precision() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%.*{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::None,
                    precision: Precision::Variable,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_float_with_star_width_and_star_precision() {
    for (format_char, specifier) in FLOATS {
        assert_eq!(
            FormatString::parse(&format!("%*.*{format_char}")),
            Ok(FormatString {
                fragments: vec![FormatFragment::Conversion(ConversionSpec {
                    flags: [].into_iter().collect(),
                    min_field_width: MinFieldWidth::Variable,
                    precision: Precision::Variable,
                    length: None,
                    specifier: specifier.clone()
                })]
            })
        );
    }
}

#[test]
fn test_char() {
    assert_eq!(
        FormatString::parse("%c"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: None,
                specifier: Specifier::Char
            })]
        })
    );
}

#[test]
fn test_char_with_minus() {
    assert_eq!(
        FormatString::parse("%-5c"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [Flag::LeftJustify].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(5),
                precision: Precision::None,
                length: None,
                specifier: Specifier::Char
            })]
        })
    );
}

#[test]
fn test_char_with_plus() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%+c").is_ok());
}

#[test]
fn test_char_with_blank_space() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("% c").is_ok());
}

#[test]
fn test_char_with_hash() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%#c").is_ok());
}

#[test]
fn test_char_with_zero() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%0c").is_ok());
}

#[test]
fn test_char_with_length() {
    // Length modifiers are ignored by %c but are still returned by the
    // parser.
    assert_eq!(
        FormatString::parse("%hhc"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Char),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse("%hc"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Short),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%lc")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Long),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%llc")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::LongLong),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%jc")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::IntMax),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%zc")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Size),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%tc")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::PointerDiff),
                specifier: Specifier::Char
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%Lc")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::LongDouble),
                specifier: Specifier::Char
            })]
        })
    );
}

#[test]
fn test_char_with_width() {
    assert_eq!(
        FormatString::parse("%5c"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(5),
                precision: Precision::None,
                length: None,
                specifier: Specifier::Char
            })]
        })
    );
}

#[test]
fn test_char_with_multidigit_width() {
    assert_eq!(
        FormatString::parse("%10c"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(10),
                precision: Precision::None,
                length: None,
                specifier: Specifier::Char
            })]
        })
    );
}

#[test]
fn test_char_with_star_width() {
    assert_eq!(
        FormatString::parse("%*c"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Variable,
                precision: Precision::None,
                length: None,
                specifier: Specifier::Char
            })]
        })
    );
}

#[test]
fn test_char_with_precision() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%.4c").is_ok());
}

#[test]
fn test_long_char_with_hash() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%#lc").is_ok());
}

#[test]
fn test_long_char_with_zero() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%0lc").is_ok());
}

#[test]
fn test_string() {
    assert_eq!(
        FormatString::parse("%s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_minus() {
    assert_eq!(
        FormatString::parse("%-6s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [Flag::LeftJustify].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(6),
                precision: Precision::None,
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_plus() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%+s").is_ok());
}

#[test]
fn test_string_with_blank_space() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("% s").is_ok());
}

#[test]
fn test_string_with_hash() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%#s").is_ok());
}

#[test]
fn test_string_with_zero() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%0s").is_ok());
}

#[test]
fn test_string_with_length() {
    // Length modifiers are ignored by %s but are still returned by the
    // parser.
    assert_eq!(
        FormatString::parse("%hhs"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Char),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse("%hs"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Short),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%ls")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Long),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%lls")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::LongLong),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%js")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::IntMax),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%zs")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::Size),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%ts")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::PointerDiff),
                specifier: Specifier::String
            })]
        })
    );

    assert_eq!(
        FormatString::parse(&format!("%Ls")),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: HashSet::new(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::None,
                length: Some(Length::LongDouble),
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_width() {
    assert_eq!(
        FormatString::parse("%6s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(6),
                precision: Precision::None,
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_multidigit_width() {
    assert_eq!(
        FormatString::parse("%10s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(10),
                precision: Precision::None,
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_star_width() {
    assert_eq!(
        FormatString::parse("%*s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Variable,
                precision: Precision::None,
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_star_precision() {
    assert_eq!(
        FormatString::parse("%.3s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::Fixed(3),
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_multidigit_precision() {
    assert_eq!(
        FormatString::parse("%.10s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::None,
                precision: Precision::Fixed(10),
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_width_and_precision() {
    assert_eq!(
        FormatString::parse("%10.3s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Fixed(10),
                precision: Precision::Fixed(3),
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_string_with_star_width_and_star_precision() {
    assert_eq!(
        FormatString::parse("%*.*s"),
        Ok(FormatString {
            fragments: vec![FormatFragment::Conversion(ConversionSpec {
                flags: [].into_iter().collect(),
                min_field_width: MinFieldWidth::Variable,
                precision: Precision::Variable,
                length: None,
                specifier: Specifier::String
            })]
        })
    );
}

#[test]
fn test_long_string_with_hash() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%#ls").is_ok());
}

#[test]
fn test_long_string_with_zero() {
    // TODO: b/281750433 - This test should fail.
    assert!(FormatString::parse("%0ls").is_ok());
}
