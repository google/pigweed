// Copyright 2025 The Pigweed Authors
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

//! Log output is base64 encoded
#![no_std]

#[doc(hidden)]
pub mod __private {
    // Re-export for use by the `pw_logf_backend!` macro.
    pub use colors::log_level_tag;
    use console;
    use pw_status::{Error, Result};
    use pw_stream::{Cursor, Write};
    use pw_tokenizer::MessageWriter;
    pub use pw_tokenizer::{tokenize_core_fmt_to_writer, tokenize_printf_to_writer};

    // Use 52 to match the default value PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES
    // on the C++ tokenizer side.
    const BUFFER_SIZE: usize = 52;

    // A simple implementation of [`pw_tokenizer::MessageWriter`] that writes
    // data to a buffer.  On message finalization, it base64 encodes the data
    // and writes it to the console.
    pub struct LogMessageWriter {
        cursor: Cursor<[u8; BUFFER_SIZE]>,
    }

    impl MessageWriter for LogMessageWriter {
        fn new() -> Self {
            Self {
                cursor: Cursor::new([0u8; BUFFER_SIZE]),
            }
        }

        fn write(&mut self, data: &[u8]) -> Result<()> {
            self.cursor.write_all(data)
        }

        fn remaining(&self) -> usize {
            self.cursor.remaining()
        }

        fn finalize(self) -> Result<()> {
            let write_len = self.cursor.position();
            let data = self.cursor.into_inner();

            // Pigweed's detokenization tools recognize base64 encoded data
            // prefixed with a `$` as tokenized data interspersed with plain text.
            // To ensure an single call to write_all(), the encode buffer is prefixed
            // with the $ and postfixed with a newline.
            // TODO: b/401562650 - implement streaming base64 encoder.
            const ENCODED_SIZE: usize = pw_base64::encoded_size(BUFFER_SIZE);
            let mut encode_buffer = [0u8; ENCODED_SIZE + 2];
            encode_buffer[0] = b'$';
            // pass a slice of encode_buffer to ensure $ is not encoded.
            let data_slice = data.get(0..write_len).ok_or(Error::OutOfRange)?;
            if let Ok(s) =
                pw_base64::encode_str(data_slice, &mut encode_buffer[1..ENCODED_SIZE + 1])
            {
                // postfix the encoded buffer with a newline after the $ and encoded string
                let encoded_len = s.len();
                if let Some(bytes_ref) = encode_buffer.get_mut(encoded_len + 1) {
                    *bytes_ref = b'\n';
                } else {
                    return Err(Error::OutOfRange);
                }
                let mut console = console::Console::new();
                let _ = console.write_all(&encode_buffer);
            } else {
                unreachable()
            }

            Ok(())
        }
    }

    fn unreachable() -> ! {
        extern "C" {
            fn pw_assert_HandleFailure() -> !;
        }

        unsafe { pw_assert_HandleFailure() };
    }
}

// Implement the `pw_log` backend API.
//
// Since we're logging to a shared/ambient resource we can use
// tokenize_*_to_writer!` instead of `tokenize_*_to_buffer!` and avoid the
// overhead of initializing any intermediate buffers or objects.
//
// Uses `pw_format` special `PW_FMT_CONCAT` operator to prepend a place to
// print the log level.
#[macro_export]
macro_rules! pw_log_backend {
  ($log_level:expr, $format_string:literal $(, $args:expr)* $(,)?) => {{
    let _ = $crate::__private::tokenize_core_fmt_to_writer!(
      $crate::__private::LogMessageWriter,
      "[{}] " PW_FMT_CONCAT $format_string,
      $crate::__private::log_level_tag($log_level) as &str,
      $($args),*);
  }};
}

#[macro_export]
macro_rules! pw_logf_backend {
  ($log_level:expr, $format_string:literal $(, $args:expr)* $(,)?) => {{
    let _ = $crate::__private::tokenize_printf_to_writer!(
      $crate::__private::LogMessageWriter,
      "[%s] " PW_FMT_CONCAT $format_string,
      $crate::__private::log_level_tag($log_level),
      $($args),*);
  }};
}
