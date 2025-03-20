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
#![no_std]

extern "C" {
    pub fn pw_assert_HandleFailure() -> !;
}

// Re-export pw_log for use by panic/assert macros.
pub mod __private {
    pub use pw_log::fatal;
}

#[cfg(feature = "color")]
#[macro_export]
macro_rules! __private_log_panic_banner {
    () => {
        // Colorized using https://glitchassassin.github.io/fk-ascii-editor/ and
        // and run throught the following shell command to translate outputs:
        //   sed 's/{70}/\\x1b[0m/'g |
        //   sed 's/{B0}/\\x1b[1;33m/'g |
        //   sed 's/{90}/\\x1b[1;31m/'g |
        //   sed 's/{E0}/\\x1b[1;36m/'g |
        //   sed 's/{C0}/\\x1b[1;34m/'g |
        //   sed 's/{A0}/\\x1b[1;32m/'g |
        //   sed 's/{F0}/\\x1b[1;37m/'g |
        //   sed 's/{D0}/\\x1b[1;35m/'g
        $crate::__private::fatal!("

\x1b[0m.-------.    ____    ,---.   .--.\x1b[1;31m.-./`)\x1b[0m     _______
\x1b[0m\\  \x1b[1;33m_(`)_\x1b[0m \\ .'  __ `. |    \\  |  |\x1b[1;31m\\\x1b[0m \x1b[1;36m.-.\x1b[1;31m')\x1b[0m   /   __  \\
\x1b[0m| \x1b[1;33m(_\x1b[0m \x1b[1;32mo\x1b[1;33m._)\x1b[0m|/   '  \\  \\|  ,  \\ |  |\x1b[1;31m/\x1b[0m \x1b[1;36m`-'\x1b[0m \x1b[1;31m\\\x1b[0m  | \x1b[1;36m,_\x1b[0m/  \\__)
\x1b[0m|  \x1b[1;33m(_,_)\x1b[0m /|___|  /  ||  |\\\x1b[1;34m_\x1b[0m \\|  | \x1b[1;31m`-'`\"`\x1b[1;36m,-./  )
\x1b[0m|   '-.-'    _.-`   ||  \x1b[1;34m_( )_\x1b[0m\\  | .---. \x1b[1;36m\\  '\x1b[1;35m_\x1b[0m \x1b[1;36m'`)
\x1b[0m|   |     .'   \x1b[1;32m_\x1b[0m    || \x1b[1;34m(_\x1b[0m \x1b[1;35mo\x1b[0m \x1b[1;34m_)\x1b[0m  | |   |  \x1b[1;36m>\x1b[0m \x1b[1;35m(_)\x1b[0m  \x1b[1;36m)\x1b[0m  __
\x1b[0m|   |     |  \x1b[1;32m_( )_\x1b[0m  ||  \x1b[1;34m(_,_)\x1b[0m\\  | |   | \x1b[1;36m(  .  .-'\x1b[0m_/  )
\x1b[0m/   )     \\ \x1b[1;32m(_\x1b[0m \x1b[1;31mo\x1b[0m \x1b[1;32m_)\x1b[0m /|  |    |  | |   |  \x1b[1;36m`-'`-'\x1b[0m     /
\x1b[0m`---'      '.\x1b[1;32m(_,_)\x1b[0m.' '--'    '--' '---'    `._____.'
\x1b[0m
")
    };
}

#[cfg(not(feature = "color"))]
#[macro_export]
macro_rules! __private_log_panic_banner {
    () => {
        $crate::__private::fatal!(
            r#"

.-------.    ____    ,---.   .--..-./`)     _______
\  _(`)_ \ .'  __ `. |    \  |  |\ .-.')   /   __  \
| (_ o._)|/   '  \  \|  ,  \ |  |/ `-' \  | ,_/  \__)
|  (_,_) /|___|  /  ||  |\_ \|  | `-'`"`,-./  )
|   '-.-'    _.-`   ||  _( )_\  | .---. \  '_ '`)
|   |     .'   _    || (_ o _)  | |   |  > (_)  )  __
|   |     |  _( )_  ||  (_,_)\  | |   | (  .  .-'_/  )
/   )     \ (_ o _) /|  |    |  | |   |  `-'`-'     /
`---'      '.(_,_).' '--'    '--' '---'    `._____.'
"#
        )
    };
}

#[macro_export]
macro_rules! panic {
  ($format_string:literal $(,)?) => {{
      // Ideally we'd combine these two log statements.  However, the `pw_log` API
      // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
      $crate::__private_log_panic_banner!();
      $crate::__private::fatal!($format_string);
      unsafe{$crate::pw_assert_HandleFailure()}
  }};

  ($format_string:literal, $($args:expr),* $(,)?) => {{
      // Ideally we'd combine these two log statements.  However, the `pw_log` API
      // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
      $crate::__private_log_panic_banner!();
      $crate::__private::fatal!($format_string, $($args),*);
      unsafe{$crate::pw_assert_HandleFailure()}
  }};
}

#[macro_export]
macro_rules! assert {
  ($condition:expr $(,)?) => {{
      if !$condition {
          // Ideally we'd combine these two log statements.  However, the `pw_log` API
          // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
          $crate::__private_log_panic_banner!();
          $crate::__private::fatal!("assert!() failed");
          unsafe{$crate::pw_assert_HandleFailure()}
      }
  }};

  ($condition:expr, $($args:expr),* $(,)?) => {{
      if !$condition {
          // Ideally we'd combine these two log statements.  However, the `pw_log` API
          // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
          $crate::__private_log_panic_banner!();
          $crate::__private::fatal!("assert!() failed");
          $crate::__private::fatal!($($args),*);
          unsafe{$crate::pw_assert_HandleFailure()}
      }
  }};
}

#[macro_export]
macro_rules! eq {
  ($condition_a:expr, $condition_b:expr $(,)?) => {{
      let a = &$condition_a;
      let b = &$condition_b;
      if *a != *b {
          // Ideally we'd combine these two log statements.  However, the `pw_log` API
          // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
          $crate::__private_log_panic_banner!();
          $crate::__private::fatal!("assert_eq!() failed, {} != {}", a, b);
          unsafe{$crate::pw_assert_HandleFailure()}
      }
  }};

  ($condition_a:expr, $condition_b:expr, $($args:expr),* $(,)?) => {{
      let a = &$condition_a;
      let b = &$condition_b;
      if *a != *b {
          // Ideally we'd combine these two log statements.  However, the `pw_log` API
          // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
          $crate::__private_log_panic_banner!();
          $crate::__private::fatal!("assert_eq!() failed, {} != {}", a, b);
          $crate::__private::fatal!($($args),*);
          unsafe{$crate::pw_assert_HandleFailure()}
      }
  }};
}

#[macro_export]
macro_rules! ne {
  ($condition_a:expr, $condition_b:expr $(,)?) => {{
      let a = &$condition_a;
      let b = &$condition_b;
      if *a == *b {
          // Ideally we'd combine these two log statements.  However, the `pw_log` API
          // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
          $crate::__private_log_panic_banner!();
          $crate::__private::fatal!("assert_neq!() failed, {} == {}", a, b);
          unsafe{$crate::pw_assert_HandleFailure()}
      }
  }};

  ($condition_a:expr, $condition_b:expr, $($args:expr),* $(,)?) => {{
      let a = &$condition_a;
      let b = &$condition_b;
      if *a == *b {
          // Ideally we'd combine these two log statements.  However, the `pw_log` API
          // does not support passing through `PW_FMT_CONCAT` tokens to `pw_format`.
          $crate::__private_log_panic_banner!();
          $crate::__private::fatal!("assert_neq!() failed, {} == {}", a, b);
          $crate::__private::fatal!($($args),*);
          unsafe{$crate::pw_assert_HandleFailure()}
      }
  }};
}
