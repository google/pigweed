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
    use console;
    use pw_status::Result;
    use pw_stream::{Cursor, Write};
    use pw_tokenizer::MessageWriter;

    // Re-export for use by the `pw_logf_backend!` macro.
    pub use colors::log_level_tag;
    pub use pw_tokenizer::{tokenize_core_fmt_to_writer, tokenize_printf_to_writer};

    const ENCODE_BUFFER_SIZE: usize = 32;

    // A simple implementation of [`pw_tokenizer::MessageWriter`] that writes
    // data to a buffer.  On message finalization, it base64 encodes the data
    // and prints it using `hprintln!`.
    pub struct LogMessageWriter {
        cursor: Cursor<[u8; ENCODE_BUFFER_SIZE]>,
    }

    impl MessageWriter for LogMessageWriter {
        fn new() -> Self {
            Self {
                cursor: Cursor::new([0u8; ENCODE_BUFFER_SIZE]),
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
            // TODO: b/401562650 - implement streaming base64 encoder.
            let mut encode_buffer = [0u8; pw_base64::encoded_size(ENCODE_BUFFER_SIZE)];
            if let Ok(s) = pw_base64::encode_str(&data[..write_len], &mut encode_buffer) {
                use embedded_io::Write;
                let mut console = console::Console::new();
                let _ = console.write(b"$");
                let _ = console.write(s.as_bytes());
                let _ = console.write(b"\n");
            }

            Ok(())
        }
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
