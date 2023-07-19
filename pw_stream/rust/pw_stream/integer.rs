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

use pw_status::Result;

/// A trait for reading integers from a stream.
///
/// Allows reading signed and unsigned integers from 8 to 128 bits in
/// either big or little endian byte order.
///
/// # Example
///
/// ```
/// use pw_stream::{Cursor, ReadInteger};
///
/// let mut cursor = Cursor::new([0x3, 0x4, 0x5, 0x80]);
/// let value = cursor.read_u32_le().unwrap();
/// assert_eq!(value, 0x8005_0403);
/// ```
///
/// # Future Work
///
/// In order to allow for optimized non-generic implementations, there is
/// no blanket implementation over the [`crate::Read`] trait.  An `IntegerReader`
/// adapter could be written to allow this functionality.
pub trait ReadInteger {
    /// Reads a little-endian i8 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i8_le(&mut self) -> Result<i8>;

    /// Reads a little-endian u8 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u8_le(&mut self) -> Result<u8>;

    /// Reads a big-endian i8 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i8_be(&mut self) -> Result<i8>;

    /// Reads a big-endian u8 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u8_be(&mut self) -> Result<u8>;

    /// Reads a little-endian i16 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i16_le(&mut self) -> Result<i16>;

    /// Reads a little-endian u16 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    ///
    fn read_u16_le(&mut self) -> Result<u16>;

    /// Reads a big-endian i16 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i16_be(&mut self) -> Result<i16>;

    /// Reads a big-endian u16 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    ///
    fn read_u16_be(&mut self) -> Result<u16>;

    /// Reads a little-endian i32 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i32_le(&mut self) -> Result<i32>;

    /// Reads a little-endian u32 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u32_le(&mut self) -> Result<u32>;

    /// Reads a big-endian i32 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i32_be(&mut self) -> Result<i32>;

    /// Reads a big-endian u32 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u32_be(&mut self) -> Result<u32>;

    /// Reads a little-endian i64 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i64_le(&mut self) -> Result<i64>;

    /// Reads a little-endian u64 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u64_le(&mut self) -> Result<u64>;

    /// Reads a big-endian i64 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i64_be(&mut self) -> Result<i64>;

    /// Reads a big-endian u64 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u64_be(&mut self) -> Result<u64>;

    /// Reads a little-endian i128 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i128_le(&mut self) -> Result<i128>;

    /// Reads a little-endian u128 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u128_le(&mut self) -> Result<u128>;

    /// Reads a big-endian i128 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_i128_be(&mut self) -> Result<i128>;

    /// Reads a big-endian u128 returning it's value or an Error.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_u128_be(&mut self) -> Result<u128>;
}

/// A trait for writing integers toa stream.
///
/// Allows writing signed and unsigned integers from 8 to 128 bits in
/// either big or little endian byte order.
///
/// # Example
///
/// ```
/// use pw_stream::{Cursor, WriteInteger};
///
/// let mut cursor = Cursor::new([0u8; 8]);
/// cursor.write_u32_le(&0x8005_0403).unwrap();
/// let buffer = cursor.into_inner();
/// assert_eq!(buffer, [0x3, 0x4, 0x5, 0x80, 0x0, 0x0, 0x0, 0x0]);
/// ```
///
/// # Future Work
///
/// In order to allow for optimized non-generic implementations, there is
/// no blanket implementation over the [`crate::Write`] trait.  An `IntegerWriter`
/// adapter could be written to allow this functionality.
pub trait WriteInteger {
    /// Writes a little-endian i8.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i8_le(&mut self, value: &i8) -> Result<()>;

    /// Writes a little-endian u8.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u8_le(&mut self, value: &u8) -> Result<()>;

    /// Writes a big-endian i8.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i8_be(&mut self, value: &i8) -> Result<()>;

    /// Writes a big-endian u8.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u8_be(&mut self, value: &u8) -> Result<()>;

    /// Writes a little-endian i16.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i16_le(&mut self, value: &i16) -> Result<()>;

    /// Writes a little-endian u16.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u16_le(&mut self, value: &u16) -> Result<()>;

    /// Writes a big-endian i16.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i16_be(&mut self, value: &i16) -> Result<()>;

    /// Writes a big-endian u16.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u16_be(&mut self, value: &u16) -> Result<()>;

    /// Writes a little-endian i32.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i32_le(&mut self, value: &i32) -> Result<()>;

    /// Writes a little-endian u32.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u32_le(&mut self, value: &u32) -> Result<()>;

    /// Writes a big-endian i32.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i32_be(&mut self, value: &i32) -> Result<()>;

    /// Writes a big-endian u32.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u32_be(&mut self, value: &u32) -> Result<()>;

    /// Writes a little-endian i64.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i64_le(&mut self, value: &i64) -> Result<()>;

    /// Writes a little-endian u64.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u64_le(&mut self, value: &u64) -> Result<()>;

    /// Writes a big-endian i64.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i64_be(&mut self, value: &i64) -> Result<()>;

    /// Writes a big-endian u64.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u64_be(&mut self, value: &u64) -> Result<()>;

    /// Writes a little-endian i128.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i128_le(&mut self, value: &i128) -> Result<()>;

    /// Writes a little-endian u128.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u128_le(&mut self, value: &u128) -> Result<()>;

    /// Writes a big-endian i128.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_i128_be(&mut self, value: &i128) -> Result<()>;

    /// Writes a big-endian u128.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_u128_be(&mut self, value: &u128) -> Result<()>;
}

/// A trait for reading varint integers from a stream.
///
/// The API here is explicitly limited to `u64` and `i64` in order to reduce
/// code size.
///
/// # Example
///
/// ```
/// use pw_stream::{Cursor, ReadVarint};
///
/// let mut cursor = Cursor::new(vec![0xfe, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x0f]);
/// let unsigned_value = cursor.read_varint().unwrap();
/// let signed_value = cursor.read_signed_varint().unwrap();
/// assert_eq!(unsigned_value, 0xffff_fffe);
/// assert_eq!(signed_value, i32::MIN.into());
/// ```
pub trait ReadVarint {
    /// Read an unsigned varint from the stream.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_varint(&mut self) -> Result<u64>;

    /// Read a signed varint from the stream.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn read_signed_varint(&mut self) -> Result<i64>;
}

/// A trait for writing varint integers from a stream.
///
/// The API here is explicitly limited to `u64` and `i64` in order to reduce
/// code size.
///
/// # Example
///
/// ```
/// use pw_stream::{Cursor, WriteVarint};
///
/// let mut cursor = Cursor::new(vec![0x0; 16]);
///
/// cursor.write_varint(0xffff_fffe).unwrap();
/// cursor.write_signed_varint(i32::MIN.into()).unwrap();
///
/// let buffer = cursor.into_inner();
/// assert_eq!(buffer,vec![
///   0xfe, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff,
///   0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]);
/// ```
pub trait WriteVarint {
    /// Write an unsigned varint to the stream.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_varint(&mut self, value: u64) -> Result<()>;

    /// Write a signed varint to the stream.
    ///
    /// Errors that may be returned are dependant on the underlying
    /// implementation.
    fn write_signed_varint(&mut self, value: i64) -> Result<()>;
}
