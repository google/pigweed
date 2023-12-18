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
#![cfg_attr(not(feature = "std"), no_std)]
#![deny(missing_docs)]

//! `pw_base64` provides simple encoding of data into base64.
//!
//! ```
//! const INPUT: &'static [u8] = "I 💖 Pigweed".as_bytes();
//!
//! // [`encoded_size`] can be used to calculate the size of the output buffer.
//! let mut output = [0u8; pw_base64::encoded_size(INPUT.len())];
//!
//! // Data can be encoded to a `&mut [u8]`.
//! let output_size = pw_base64::encode(INPUT, &mut output).unwrap();
//! assert_eq!(&output[0..output_size], b"SSDwn5KWIFBpZ3dlZWQ=");
//!
//! // The output buffer can also be automatically converted to a `&str`.
//! let output_str = pw_base64::encode_str(INPUT, &mut output).unwrap();
//! assert_eq!(output_str, "SSDwn5KWIFBpZ3dlZWQ=");
//! ```

use pw_status::{Error, Result};
use pw_stream::{Cursor, ReadInteger, Seek, Write};

// Helper macro to make declaring the base 64 encode table more concise.
macro_rules! b {
    ($char:tt) => {
        stringify!($char).as_bytes()[0]
    };
}

// We use `u8`s in our encoding table instead of `char`s in order to avoid the
// overhead of 1) storing each entry as 4 bytes and 2) overhead of converting
// from `char` to `u8` while building the output.
//
// When constructing this table, the `b!` macro makes the assumption that
// all the characters are a single byte in utf8.  This is true as base64
// only outputs ASCII characters.
#[rustfmt::skip]
const BASE64_ENCODE_TABLE: [u8; 64] = [
    b!(A), b!(B), b!(C), b!(D), b!(E), b!(F), b!(G), b!(H),
    b!(I), b!(J), b!(K), b!(L), b!(M), b!(N), b!(O), b!(P),
    b!(Q), b!(R), b!(S), b!(T), b!(U), b!(V), b!(W), b!(X),
    b!(Y), b!(Z), b!(a), b!(b), b!(c), b!(d), b!(e), b!(f),
    b!(g), b!(h), b!(i), b!(j), b!(k), b!(l), b!(m), b!(n),
    b!(o), b!(p), b!(q), b!(r), b!(s), b!(t), b!(u), b!(v),
    b!(w), b!(x), b!(y), b!(z), b!(0), b!(1), b!(2), b!(3),
    b!(4), b!(5), b!(6), b!(7), b!(8), b!(9), b!(+), b!(/),
];
const BASE64_PADDING: u8 = b!(=);

/// Returns the size of the output buffer needed to encode an input buffer of
/// size `input_size`.
pub const fn encoded_size(input_size: usize) -> usize {
    (input_size + 2) / 3 * 4 // +2 to round up to a 3-byte group
}

// Base 64 encoding represents every 3 bytes with 4 ascii characters.  Each
// of these 4 ascii characters represents 6 bits of data from the 3 bytes of
// input.  The below helpers calculate each of the 4 characters form the 3 bytes
// of input.
const fn char_0(b: &[u8; 3]) -> u8 {
    BASE64_ENCODE_TABLE[((b[0] & 0b11111100) >> 2) as usize]
}

const fn char_1(b: &[u8; 3]) -> u8 {
    BASE64_ENCODE_TABLE[(((b[0] & 0b00000011) << 4) | ((b[1] & 0b11110000) >> 4)) as usize]
}

const fn char_2(b: &[u8; 3]) -> u8 {
    BASE64_ENCODE_TABLE[(((b[1] & 0b00001111) << 2) | ((b[2] & 0b11000000) >> 6)) as usize]
}

const fn char_3(b: &[u8; 3]) -> u8 {
    BASE64_ENCODE_TABLE[(b[2] & 0b00111111) as usize]
}

/// Encode `input` as base64 into the `output_buffer`.
///
/// Returns the number of bytes written to `output_buffer` on success or
/// `Error::OutOfRange` if `output_buffer` is not large enough.
pub fn encode(input: &[u8], output: &mut [u8]) -> Result<usize> {
    if output.len() < encoded_size(input.len()) {
        return Err(Error::OutOfRange);
    }
    let mut input = Cursor::new(input);
    let mut output = Cursor::new(output);

    let mut remaining_bytes = input.len();
    while remaining_bytes > 0 {
        let bytes = [
            input.read_u8_le().unwrap_or(0),
            input.read_u8_le().unwrap_or(0),
            input.read_u8_le().unwrap_or(0),
        ];

        output.write(&[
            char_0(&bytes),
            char_1(&bytes),
            if remaining_bytes > 1 {
                char_2(&bytes)
            } else {
                BASE64_PADDING
            },
            if remaining_bytes > 2 {
                char_3(&bytes)
            } else {
                BASE64_PADDING
            },
        ])?;
        remaining_bytes = remaining_bytes.saturating_add_signed(-3);
    }

    output.stream_position().map(|len| len as usize)
}

/// Encode `input` as base64 into `output_buffer` and interprets it as a
/// string.
///
/// Returns a `&str` referencing the `output_buffer` buffer on success or
/// `Error::OutOfRange` if `output_buffer` is not large enough.
///
/// Using this method avoids having to do unicode checking as it can guarantee
/// that the data written to `output_buffer` is only valid ASCII bytes.
pub fn encode_str<'a>(input: &[u8], output_buffer: &'a mut [u8]) -> Result<&'a str> {
    let encode_len = encode(input, output_buffer)?;
    // Safety: Since we are building the output buffer strictly from ASCII
    // characters, it is guaranteed to be a valid string.
    unsafe {
        Ok(core::str::from_utf8_unchecked(
            &output_buffer[0..encode_len],
        ))
    }
}

#[cfg(test)]
mod tests;
