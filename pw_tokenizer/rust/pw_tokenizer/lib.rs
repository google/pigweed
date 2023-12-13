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

//! `pw_tokenizer` - Efficient string handling and printf style encoding.
//!
//! Logging is critical, but developers are often forced to choose between
//! additional logging or saving crucial flash space. The `pw_tokenizer` crate
//! helps address this by replacing printf-style strings with binary tokens
//! during compilation. This enables extensive logging with substantially less
//! memory usage.
//!
//! For a more in depth explanation of the systems design and motivations,
//! see [Pigweed's pw_tokenizer module documentation](https://pigweed.dev/pw_tokenizer/).
//!
//! # Example
//!
//! ```
//! use pw_tokenizer::tokenize_to_buffer;
//!
//! # fn doctest() -> pw_status::Result<()> {
//! let mut buffer = [0u8; 1024];
//! let len = tokenize_to_buffer!(&mut buffer, "The answer is %d", 42)?;
//!
//! // 4 bytes used to encode the token and one to encode the value 42.  This
//! // is a **3.5x** reduction in size compared to the raw string!
//! assert_eq!(len, 5);
//! # Ok(())
//! # }
//! # doctest().unwrap();
//! ```

#![no_std]
#![deny(missing_docs)]

#[doc(hidden)]
pub mod internal;

#[doc(hidden)]
// Creating a __private namespace allows us a way to get to the modules
// we need from macros by doing:
//     use $crate::__private as __pw_tokenizer_crate;
//
// This is how proc macro generated code can reliably reference back to
// `pw_tokenizer` while still allowing a user to import it under a different
// name.
pub mod __private {
    pub use crate::*;
    pub use pw_status::Result;
    pub use pw_stream::{Cursor, Seek, WriteInteger, WriteVarint};
    pub use pw_tokenizer_macro::{_token, _tokenize_to_buffer};
}

/// Return the [`u32`] token for the specified string and add it to the token
/// database.
///
/// This is where the magic happens in `pw_tokenizer`!   ... and by magic
/// we mean hiding information in a special linker section that ends up in the
/// final elf binary but does not get flashed to the device.
///
/// Two things are accomplished here:
/// 1) The string is hashed into its stable `u32` token.  This is the value that
///    is returned from the macro.
/// 2) A [token database entry](https://pigweed.dev/pw_tokenizer/design.html#binary-database-format)
///   is generated, assigned to a unique static symbol, placed in a linker
///   section named `pw_tokenizer.entries.<TOKEN_HASH>`.  A
///   [linker script](https://pigweed.googlesource.com/pigweed/pigweed/+/refs/heads/main/pw_tokenizer/pw_tokenizer_linker_sections.ld)
///   is responsible for picking these symbols up and aggregating them into a
///   single `.pw_tokenizer.entries` section in the final binary.
///
/// # Example
/// ```
/// use pw_tokenizer::token;
///
/// let token = token!("hello, \"world\"");
/// assert_eq!(token, 3537412730);
/// ```
///
/// Currently there is no support for encoding tokens to specific domains
/// or with "fixed lengths" per [`pw_tokenizer_core::hash_bytes_fixed`].
#[macro_export]
macro_rules! token {
    ($string:literal) => {{
        $crate::__private::_token!($string)
    }};
}

/// Tokenize a format string and arguments to an [`AsMut<u8>`] buffer and add
/// the format string's token to the token database.
///
/// See [`token`] for an explanation on how strings are tokenized and entries
/// are added to the token database.
///
/// Returns a [`pw_status::Result<usize>`] the number of bytes written to the buffer.
///
/// # Errors
/// - [`pw_status::Error::OutOfRange`] - Buffer is not large enough to fit
///   tokenized data.
/// - [`pw_status::Error::InvalidArgument`] - Invalid buffer was provided.
///
/// # Example
///
/// ```
/// use pw_tokenizer::tokenize_to_buffer;
///
/// # fn doctest() -> pw_status::Result<()> {
/// let mut buffer = [0u8; 1024];
/// let len = tokenize_to_buffer!(&mut buffer, "The answer is %d", 42)?;
///
/// // 4 bytes used to encode the token and one to encode the value 42.
/// assert_eq!(len, 5);
/// # Ok(())
/// # }
/// # doctest().unwrap();
/// ```
#[macro_export]
macro_rules! tokenize_to_buffer {
    ($buffer:expr, $format_string:literal) => {{
      use $crate::__private as __pw_tokenizer_crate;
      __pw_tokenizer_crate::_tokenize_to_buffer!($buffer, $format_string)
    }};

    ($buffer:expr, $format_string:expr, $($args:expr),*) => {{
      use $crate::__private as __pw_tokenizer_crate;
      __pw_tokenizer_crate::_tokenize_to_buffer!($buffer, $format_string, $($args),*)
    }};
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate self as pw_tokenizer;

    // This is not meant to be an exhaustive test of tokenization which is
    // covered by `pw_tokenizer_core`'s unit tests.  Rather, this is testing
    // that the `tokenize!` macro connects to that correctly.
    #[test]
    fn test_token() {}

    macro_rules! tokenize_to_buffer_test {
      ($expected_data:expr, $buffer_len:expr, $fmt:expr) => {
        {
          let mut orig_buffer = [0u8; $buffer_len];
          let buffer =
              tokenize_to_buffer!(&mut orig_buffer, $fmt).unwrap();
            let len = buffer.len();
            assert_eq!(
              &orig_buffer[..(($buffer_len) - len)],
              $expected_data,
          );
        }
      };

      ($expected_data:expr, $buffer_len:expr, $fmt:expr, $($args:expr),*) => {
        {
          let mut buffer = [0u8; $buffer_len];
          let len = tokenize_to_buffer!(&mut buffer, $fmt, $($args),*).unwrap();
          assert_eq!(
              &buffer[..len],
              $expected_data,
          );
        }
      };
    }

    #[test]
    fn test_decimal_format() {
        tokenize_to_buffer_test!(
            &[0x52, 0x1c, 0xb0, 0x4c, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %d!",
            1
        );

        tokenize_to_buffer_test!(
            &[0x36, 0xd0, 0xfb, 0x69, 0x1], // expected buffer
            64,                             // buffer size
            "No! The answer is %d!",
            -1
        );

        tokenize_to_buffer_test!(
            &[0xa4, 0xad, 0x50, 0x54, 0x0], // expected buffer
            64,                             // buffer size
            "I think you'll find that the answer is %d!",
            0
        );
    }

    #[test]
    fn test_misc_integer_format() {
        // %d, %i, %o, %u, %x, %X all encode integers the same.
        tokenize_to_buffer_test!(
            &[0x52, 0x1c, 0xb0, 0x4c, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %d!",
            1
        );

        // Because %i is an alias for %d, it gets converted to a %d by the
        // `pw_format` macro infrastructure.
        tokenize_to_buffer_test!(
            &[0x52, 0x1c, 0xb0, 0x4c, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %i!",
            1
        );

        tokenize_to_buffer_test!(
            &[0x5d, 0x70, 0x12, 0xb4, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %o!",
            1u32
        );

        tokenize_to_buffer_test!(
            &[0x63, 0x58, 0x5f, 0x8f, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %u!",
            1u32
        );

        tokenize_to_buffer_test!(
            &[0x66, 0xcc, 0x05, 0x7d, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %x!",
            1u32
        );

        tokenize_to_buffer_test!(
            &[0x46, 0x4c, 0x16, 0x96, 0x2], // expected buffer
            64,                             // buffer size
            "The answer is %X!",
            1u32
        );
    }

    #[test]
    fn test_string_format() {
        tokenize_to_buffer_test!(
            b"\x25\xf6\x2e\x66\x07Pigweed", // expected buffer
            64,                             // buffer size
            "Hello: %s!",
            "Pigweed"
        );
    }

    #[test]
    fn test_string_format_overflow() {
        tokenize_to_buffer_test!(
            b"\x25\xf6\x2e\x66\x83Pig", // expected buffer
            8,                          // buffer size
            "Hello: %s!",
            "Pigweed"
        );
    }

    #[test]
    fn test_char_format() {
        tokenize_to_buffer_test!(
            &[0x2e, 0x52, 0xac, 0xe4, 0x50], // expected buffer
            64,                              // buffer size
            "Hello: %cigweed",
            "P".as_bytes()[0]
        );
    }
}
