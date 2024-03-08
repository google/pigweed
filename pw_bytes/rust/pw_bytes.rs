// Copyright 2024 The Pigweed Authors
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

//! pw_bytes is a collection of utilities for manipulating binary data.
//!
//! # Features
//! pw_bytes contains the follow features:
//! * macros for concatenating `const [u8]`s and `&'static str`s.
//!
//! # Examples
//! ```
//! use pw_bytes::concat_const_u8_slices;
//!
//! // Concatenate two slices.
//! const SLICE_A: &[u8] = b"abc";
//! const SLICE_B: &[u8] = b"def";
//! const SLICE_AB: &[u8] = concat_const_u8_slices!(SLICE_A, SLICE_B);
//! assert_eq!(SLICE_AB, b"abcdef");
//! ```
//!
//! ```
//! use pw_bytes::concat_static_strs;
//!
//! // Concatenate two strings.
//! const STR_A: &'static str = "abc";
//! const STR_B: &'static str = "def";
//! const STR_AB: &'static str = concat_static_strs!(STR_A, STR_B);
//! assert_eq!(STR_AB, "abcdef");
//!
//! ```
#![no_std]
#![deny(missing_docs)]

/// Concatenates multiple `const [u8]`s into one.
///
/// Returns a `const [u8]`
#[macro_export]
macro_rules! concat_const_u8_slices {
  ($($slice:expr),+) => {{
      // Calculate the length of the resulting array.  Because `+` is not a
      // valid `MacroRepSep` (see https://doc.rust-lang.org/reference/macros-by-example.html#macros-by-example),
      // we must precede the variadic expansion with a 0 and embed the + in
      // the expansion itself.
      const TOTAL_LEN: usize = 0 $(+ $slice.len())+;
      const ARRAY: [u8; TOTAL_LEN] = {
          let mut array = [0u8; TOTAL_LEN];
          let mut array_index = 0;

          // For each input slice, copy its contents into `array`.
          $({
              // Using while loop as for loops are not allowed in `const` expressions
              let mut slice_index = 0;
              while slice_index < $slice.len() {
                  array[array_index] = $slice[slice_index];
                  array_index += 1;
                  slice_index += 1;
              }
          })+;

          array
      };
      &ARRAY
  }}
}

/// Concatenates multiple `const &'static str`s into one.
///
/// Returns a `const &'static str`
#[macro_export]
macro_rules! concat_static_strs {
  ($($string:expr),+) => {{
    // Safety: we're building a byte array of known valid utf8 strings so the
    // resulting string is guaranteed to be valid utf8.
    unsafe{
      core::str::from_utf8_unchecked($crate::concat_const_u8_slices!($($string.as_bytes()),+))
    }
  }}
}

#[cfg(test)]
mod tests {
    #[test]
    fn one_const_slice_concatenates_correctly() {
        const SLICE_A: &[u8] = b"abc";
        const SLICE_A_PRIME: &[u8] = concat_const_u8_slices!(SLICE_A);
        assert_eq!(SLICE_A_PRIME, b"abc");
    }

    #[test]
    fn two_const_slices_concatenates_correctly() {
        const SLICE_A: &[u8] = b"abc";
        const SLICE_B: &[u8] = b"def";
        const SLICE_AB: &[u8] = concat_const_u8_slices!(SLICE_A, SLICE_B);
        assert_eq!(SLICE_AB, b"abcdef");
    }

    #[test]
    fn three_const_slices_concatenates_correctly() {
        const SLICE_A: &[u8] = b"abc";
        const SLICE_B: &[u8] = b"def";
        const SLICE_C: &[u8] = b"ghi";
        const SLICE_ABC: &[u8] = concat_const_u8_slices!(SLICE_A, SLICE_B, SLICE_C);
        assert_eq!(SLICE_ABC, b"abcdefghi");
    }

    #[test]
    fn empty_first_const_slice_concatenates_correctly() {
        const SLICE_A: &[u8] = b"";
        const SLICE_B: &[u8] = b"def";
        const SLICE_C: &[u8] = b"ghi";
        const SLICE_ABC: &[u8] = concat_const_u8_slices!(SLICE_A, SLICE_B, SLICE_C);
        assert_eq!(SLICE_ABC, b"defghi");
    }

    #[test]
    fn empty_middle_const_slice_concatenates_correctly() {
        const SLICE_A: &[u8] = b"abc";
        const SLICE_B: &[u8] = b"";
        const SLICE_C: &[u8] = b"ghi";
        const SLICE_ABC: &[u8] = concat_const_u8_slices!(SLICE_A, SLICE_B, SLICE_C);
        assert_eq!(SLICE_ABC, b"abcghi");
    }

    #[test]
    fn empty_last_const_slice_concatenates_correctly() {
        const SLICE_A: &[u8] = b"abc";
        const SLICE_B: &[u8] = b"def";
        const SLICE_C: &[u8] = b"";
        const SLICE_ABC: &[u8] = concat_const_u8_slices!(SLICE_A, SLICE_B, SLICE_C);
        assert_eq!(SLICE_ABC, b"abcdef");
    }

    #[test]
    // Since `concat_static_strs!` uses `concat_const_u8_slices!`, we rely on
    // the exhaustive tests above for testing edge conditions.
    fn strings_concatenates_correctly() {
        const STR_A: &'static str = "abc";
        const STR_B: &'static str = "def";
        const STR_AB: &'static str = concat_static_strs!(STR_A, STR_B);
        assert_eq!(STR_AB, "abcdef");
    }
}
