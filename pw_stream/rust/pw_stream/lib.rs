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

use core::cmp::min;

use pw_status::{Error, Result};

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

/// Wraps an <code>[AsRef]<[u8]></code> in a container implementing
/// [`Read`], [`Write`], and [`Seek`].
///
/// [`Write`] support requires the inner type also implement
/// <code>[AsMut]<[u8]></code>.
pub struct Cursor<T>
where
    T: AsRef<[u8]>,
{
    inner: T,
    pos: usize,
}

impl<T: AsRef<[u8]>> Cursor<T> {
    /// Create a new Cursor wrapping `inner` with an initial position of 0.
    ///
    /// Semantics match [`std::io::Cursor::new()`].
    pub fn new(inner: T) -> Self {
        Self { inner, pos: 0 }
    }

    /// Consumes the cursor and returns the inner wrapped data.
    pub fn into_inner(self) -> T {
        self.inner
    }

    fn remaining(&self) -> usize {
        self.len() - self.pos
    }

    fn len(&self) -> usize {
        self.inner.as_ref().len()
    }
}

impl<T: AsRef<[u8]>> Read for Cursor<T> {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        let read_len = min(self.remaining(), buf.len());
        buf[..read_len].copy_from_slice(&self.inner.as_ref()[self.pos..(self.pos + read_len)]);
        self.pos += read_len;
        Ok(read_len)
    }
}

impl<T: AsRef<[u8]> + AsMut<[u8]>> Write for Cursor<T> {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        let write_len = min(self.remaining(), buf.len());
        self.inner.as_mut()[self.pos..(self.pos + write_len)].copy_from_slice(&buf[0..write_len]);
        self.pos += write_len;
        Ok(write_len)
    }

    fn flush(&mut self) -> Result<()> {
        // Cursor does not provide any buffering so flush() is a noop.
        Ok(())
    }
}

impl<T: AsRef<[u8]>> Seek for Cursor<T> {
    fn seek(&mut self, pos: SeekFrom) -> Result<u64> {
        let new_pos = match pos {
            SeekFrom::Start(pos) => pos,
            SeekFrom::Current(pos) => (self.pos as u64)
                .checked_add_signed(pos)
                .ok_or(Error::OutOfRange)?,
            SeekFrom::End(pos) => (self.len() as u64)
                .checked_add_signed(-pos)
                .ok_or(Error::OutOfRange)?,
        };

        // Since Cursor operates on in memory buffers, it's limited by usize.
        // Return an error if we are asked to seek beyond that limit.
        let new_pos: usize = new_pos.try_into().map_err(|_| Error::OutOfRange)?;

        if new_pos > self.len() {
            Err(Error::OutOfRange)
        } else {
            self.pos = new_pos;
            Ok(new_pos as u64)
        }
    }

    // Implement more efficient versions of rewind, stream_len, stream_position.
    fn rewind(&mut self) -> Result<()> {
        self.pos = 0;
        Ok(())
    }

    fn stream_len(&mut self) -> Result<u64> {
        Ok(self.len() as u64)
    }

    fn stream_position(&mut self) -> Result<u64> {
        Ok(self.pos as u64)
    }
}

#[cfg(test)]
mod tests {
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

    fn test_rewind_resets_position_to_zero<const LEN: u64, T: Seek>(mut seeker: T) {
        seeker.seek(SeekFrom::Current(LEN as i64 / 2)).unwrap();
        assert_eq!(seeker.stream_position().unwrap(), LEN / 2);
        seeker.rewind().unwrap();
        assert_eq!(seeker.stream_position().unwrap(), 0);
    }

    fn test_stream_pos_reports_correct_position<const LEN: u64, T: Seek>(mut seeker: T) {
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

    fn test_stream_len_reports_correct_length<const LEN: u64, T: Seek>(mut seeker: T) {
        assert_eq!(seeker.stream_len().unwrap(), LEN);
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

    #[test]
    fn cursor_remaining_returns_remaining_bytes() {
        let cursor = Cursor {
            inner: &[0u8; 64],
            pos: 32,
        };
        assert_eq!(cursor.remaining(), 32);
    }

    #[test]
    fn cursor_read_of_partial_buffer_reads_correct_data() {
        let mut cursor = Cursor {
            inner: &[1, 2, 3, 4, 5, 6, 7, 8],
            pos: 4,
        };
        let mut buf = [0u8; 8];
        assert_eq!(cursor.read(&mut buf), Ok(4));
        assert_eq!(buf, [5, 6, 7, 8, 0, 0, 0, 0]);
    }

    #[test]
    fn cursor_write_of_partial_buffer_writes_correct_data() {
        let mut cursor = Cursor {
            inner: &mut [0, 0, 0, 0, 0, 0, 0, 0],
            pos: 4,
        };
        let mut buf = [1, 2, 3, 4, 5, 6, 7, 8];
        assert_eq!(cursor.write(&mut buf), Ok(4));
        assert_eq!(cursor.inner, &[0, 0, 0, 0, 1, 2, 3, 4]);
    }

    #[test]
    fn cursor_rewind_resets_position_to_zero() {
        test_rewind_resets_position_to_zero::<64, _>(Cursor::new(&[0u8; 64]));
    }

    #[test]
    fn cursor_stream_pos_reports_correct_position() {
        test_stream_pos_reports_correct_position::<64, _>(Cursor::new(&[0u8; 64]));
    }

    #[test]
    fn cursor_stream_len_reports_correct_length() {
        test_stream_len_reports_correct_length::<64, _>(Cursor::new(&[0u8; 64]));
    }
}
