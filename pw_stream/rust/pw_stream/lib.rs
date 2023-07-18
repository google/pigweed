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

//! `pw_stream` provides `no_std` versions of Rust's [`std::io::Read`],
//! [`std::io::Write`], and [`std::io::Seek`] traits as well as a simplified
//! version of [`std::io::Cursor`].  One notable difference is that
//! [`pw_status::Error`] is used to avoid needing to do error conversion or
//! encapsulation.
#![deny(missing_docs)]
// Allows docs to reference `std`
#![cfg_attr(not(feature = "std"), no_std)]

use pw_status::Result;

#[doc(hidden)]
mod cursor;
mod integer;

pub use cursor::Cursor;
pub use integer::{ReadInteger, WriteInteger};

/// A trait for objects that provide streaming read capability.
pub trait Read {
    /// Read from a stream into a buffer.
    ///
    /// Semantics match [`std::io::Read::read()`].
    fn read(&mut self, buf: &mut [u8]) -> Result<usize>;
}

/// A trait for objects that provide streaming write capability.
pub trait Write {
    /// Write a buffer to a stream.
    ///
    /// Semantics match [`std::io::Write::write()`].
    fn write(&mut self, buf: &[u8]) -> Result<usize>;

    /// Commit any outstanding buffered writes to underlying storage.
    ///
    /// Semantics match [`std::io::Write::flush()`].
    fn flush(&mut self) -> Result<()>;
}

/// A description of a seek operation in a stream.
///
/// While `pw_stream` targets embedded platforms which are often natively
/// 32 bit, we believe that seek operation are relatively rare and the added
/// overhead of using 64 bit values for seeks is balanced by the ability
/// to support objects and operations over 4 GiB.
pub enum SeekFrom {
    /// Seek from the start of the stream.
    Start(u64),

    /// Seek from the end of the stream.
    End(i64),

    /// Seek from the current position of the stream.
    Current(i64),
}

/// A trait for objects that provide the ability to seek withing a stream.
pub trait Seek {
    /// Adjust the current position of the stream.
    ///
    /// Semantics match [`std::io::Seek::seek()`].
    fn seek(&mut self, pos: SeekFrom) -> Result<u64>;

    /// Set the current position of the stream to its beginning.
    ///
    /// Semantics match [`std::io::Seek::rewind()`].
    fn rewind(&mut self) -> Result<()> {
        self.seek(SeekFrom::Start(0)).map(|_| ())
    }

    /// Returns the length of the stream.
    ///
    /// Semantics match [`std::io::Seek::stream_len()`].
    fn stream_len(&mut self) -> Result<u64> {
        // Save original position.
        let orig_pos = self.seek(SeekFrom::Current(0))?;

        // Seed to the end to discover stream length.
        let end_pos = self.seek(SeekFrom::End(0))?;

        // Go back to original position.
        self.seek(SeekFrom::Start(orig_pos))?;

        Ok(end_pos)
    }

    /// Returns the current position of the stream.
    ///
    /// Semantics match [`std::io::Seek::stream_position()`].
    fn stream_position(&mut self) -> Result<u64> {
        self.seek(SeekFrom::Current(0))
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use super::{Seek, SeekFrom};

    pub(crate) fn test_rewind_resets_position_to_zero<const LEN: u64, T: Seek>(mut seeker: T) {
        seeker.seek(SeekFrom::Current(LEN as i64 / 2)).unwrap();
        assert_eq!(seeker.stream_position().unwrap(), LEN / 2);
        seeker.rewind().unwrap();
        assert_eq!(seeker.stream_position().unwrap(), 0);
    }

    pub(crate) fn test_stream_pos_reports_correct_position<const LEN: u64, T: Seek>(mut seeker: T) {
        assert_eq!(seeker.stream_position().unwrap(), 0);
        seeker.seek(SeekFrom::Current(1)).unwrap();
        assert_eq!(seeker.stream_position().unwrap(), 1);
        seeker.seek(SeekFrom::Current(LEN as i64 / 2 - 1)).unwrap();
        assert_eq!(seeker.stream_position().unwrap(), LEN / 2);
        seeker.seek(SeekFrom::Current(0)).unwrap();
        assert_eq!(seeker.stream_position().unwrap(), LEN / 2);
        seeker.seek(SeekFrom::End(0)).unwrap();
        assert_eq!(seeker.stream_position().unwrap(), LEN);
    }

    pub(crate) fn test_stream_len_reports_correct_length<const LEN: u64, T: Seek>(mut seeker: T) {
        assert_eq!(seeker.stream_len().unwrap(), LEN);
    }
}

#[cfg(test)]
mod tests {
    use pw_status::Error;

    use super::test_utils::*;
    use super::*;

    struct TestSeeker {
        len: u64,
        pos: u64,
    }

    impl Seek for TestSeeker {
        fn seek(&mut self, pos: SeekFrom) -> Result<u64> {
            let new_pos = match pos {
                SeekFrom::Start(pos) => pos,
                SeekFrom::Current(pos) => {
                    self.pos.checked_add_signed(pos).ok_or(Error::OutOfRange)?
                }
                SeekFrom::End(pos) => self.len.checked_add_signed(-pos).ok_or(Error::OutOfRange)?,
            };

            if new_pos > self.len {
                Err(Error::OutOfRange)
            } else {
                self.pos = new_pos;
                Ok(new_pos)
            }
        }
    }

    #[test]
    fn default_rewind_impl_resets_position_to_zero() {
        test_rewind_resets_position_to_zero::<64, _>(TestSeeker { len: 64, pos: 0 });
    }

    #[test]
    fn default_stream_pos_impl_reports_correct_position() {
        test_stream_pos_reports_correct_position::<64, _>(TestSeeker { len: 64, pos: 0 });
    }

    #[test]
    fn default_stream_len_impl_reports_correct_length() {
        test_stream_len_reports_correct_length::<64, _>(TestSeeker { len: 64, pos: 32 });
    }
}
