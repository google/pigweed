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

use core::cmp::min;

use pw_status::Result;
use pw_stream::{Cursor, Write};

pub fn encode_string(cursor: &mut Cursor<&mut [u8]>, value: &str) -> Result<()> {
    const MAX_STRING_LENGTH: usize = 0x7f;

    let string_bytes = value.as_bytes();

    // Limit the encoding to the lesser of 127 or the available space in the buffer.
    let max_len = min(MAX_STRING_LENGTH, cursor.remaining() - 1);
    let overflow = max_len < string_bytes.len();
    let len = min(max_len, string_bytes.len());

    // First byte of an encoded string is it's length.
    let mut header = len as u8;

    // The high bit of the first byte is used to indicate if the string was
    // truncated.
    if overflow {
        header |= 0x80;
    }
    cursor.write_all(&[header as u8])?;

    cursor.write_all(&string_bytes[..len])
}

#[cfg(test)]
mod test {
    use pw_stream::{Cursor, Seek};

    use super::encode_string;

    fn do_string_encode_test<const BUFFER_LEN: usize>(value: &str, expected: &[u8]) {
        let mut buffer = [0u8; BUFFER_LEN];
        let mut cursor = Cursor::new(&mut buffer[..]);
        encode_string(&mut cursor, value).unwrap();

        let len = cursor.stream_position().unwrap() as usize;
        let buffer = cursor.into_inner();

        assert_eq!(len, expected.len());
        assert_eq!(&buffer[..len], expected);
    }

    #[test]
    fn test_string_encode() {
        do_string_encode_test::<64>("test", b"\x04test");
        do_string_encode_test::<4>("test", b"\x83tes");
        do_string_encode_test::<1>("test", b"\x80");

        // Truncates when the string does not fit.
        do_string_encode_test::<64>(
            "testtesttesttesttesttesttesttesttesttesttesttesttesttesttesttest",
            b"\xbftesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttes",
        );

        // Truncates when string is over 127 bytes.
        do_string_encode_test::<1024>(
            "testtesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttest",
            b"\xfftesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttesttes",
        );
    }
}
