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

//! # pw_tokenizer_core
//!
//! This crate provides the core functionality of calculating a token hash
//! for a string or byte sequence.  This is intended to provide a minimal core
//! for both the main `pw_tokenizer` and `pw_tokenizer_macro` crates.  Users
//! should prefer depending `pw_tokenizer` instead of this crate.

use core::num::Wrapping;

pub const HASH_CONSTANT: Wrapping<u32> = Wrapping(65599u32);

/// Calculate the hash for a sequence of bytes.
///
/// ```
/// use pw_tokenizer_core::hash_bytes;
///
/// let hash = hash_bytes(&[0x34, 0xd8, 0x3a, 0xbb, 0xf1, 0x0e, 0x07]);
/// assert_eq!(hash, 0x9e624642);
/// ```
pub fn hash_bytes(bytes: &[u8]) -> u32 {
    hash_bytes_fixed(bytes, bytes.len())
}

/// Calculate the hash for a sequence of bytes, examining at most `len` bytes.
///
/// ```
/// use pw_tokenizer_core::hash_bytes_fixed;
///
/// let hash = hash_bytes_fixed(&[0x34, 0xd8, 0x3a, 0xbb, 0xf1, 0x0e, 0x07], 4);
/// assert_eq!(hash, 0x92c5d2ac);
/// ```
pub fn hash_bytes_fixed(bytes: &[u8], len: usize) -> u32 {
    // The length is hashed as if it were the first byte.
    let mut hash = Wrapping(bytes.len() as u32);
    let mut coefficient = HASH_CONSTANT;

    // Fixed sized hashes are seeded with the total length of the slice
    // but only hash over at most `len` bytes.
    let bytes = if bytes.len() > len {
        &bytes[0..len]
    } else {
        bytes
    };

    // Hash all of the bytes in the slice as u32s.  Wrapping arithmetic
    // is used intentionally as part of the hashing algorithm.
    for b in bytes {
        hash += coefficient * Wrapping(*b as u32);
        coefficient *= HASH_CONSTANT;
    }

    hash.0
}

/// Calculate the hash for a string.
///
/// ```
/// use pw_tokenizer_core::hash_string;
///
/// let hash = hash_string("I 💖 Pigweed");
/// assert_eq!(hash, 0xe318d1b3);
/// ```
pub fn hash_string(s: &str) -> u32 {
    hash_bytes(s.as_bytes())
}

pub const TOKENIZER_ENTRY_MAGIC: u32 = 0xBAA98DEE;

#[cfg(test)]
mod tests {
    use super::hash_bytes_fixed;

    struct TestCase {
        string: &'static [u8],
        hash_length: usize,
        hash: u32,
    }

    // Generated file defines `test_cases()`.
    include!("pw_tokenizer_core_test_cases.rs");

    #[test]
    fn hash() {
        for test in test_cases() {
            let hash = hash_bytes_fixed(test.string, test.hash_length);
            assert_eq!(
                hash, test.hash,
                "{:08x} != {:08x} string: {:x?} hash_size: {}",
                hash, test.hash, test.string, test.hash_length
            );
        }
    }
}
