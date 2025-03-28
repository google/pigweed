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
#![no_main]

use unittest::test;

// Because infrastructure to verify panics does not exist, these tests only
// check for the valid condition and the syntax of the macros being correct.

#[test]
fn assert_syntax_works() -> unittest::Result<()> {
    pw_assert::assert!(true as bool);
    pw_assert::assert!(true as bool,);

    pw_assert::assert!(true as bool, "custom msg");
    pw_assert::assert!(true as bool, "custom msg",);

    pw_assert::assert!(true as bool, "custom msg with arg {}", 42 as u32);
    pw_assert::assert!(true as bool, "custom msg with arg {}", 42 as u32,);
    Ok(())
}

#[test]
fn assert_eq_syntax_works() -> unittest::Result<()> {
    pw_assert::eq!(1 as u32, 1 as u32);
    pw_assert::eq!(1 as u32, 1 as u32,);

    pw_assert::eq!(1 as u32, 1 as u32, "custom msg");
    pw_assert::eq!(1 as u32, 1 as u32, "custom msg",);

    pw_assert::eq!(1 as u32, 1 as u32, "custom msg with arg {}", 42 as u32);
    pw_assert::eq!(1 as u32, 1 as u32, "custom msg with arg {}", 42 as u32,);
    Ok(())
}

#[test]
fn assert_ne_syntax_works() -> unittest::Result<()> {
    pw_assert::ne!(1 as u32, 2 as u32);
    pw_assert::ne!(1 as u32, 2 as u32,);

    pw_assert::ne!(1 as u32, 2 as u32, "custom msg");
    pw_assert::ne!(1 as u32, 2 as u32, "custom msg",);

    pw_assert::ne!(1 as u32, 2 as u32, "custom msg with arg {}", 42 as u32);
    pw_assert::ne!(1 as u32, 2 as u32, "custom msg with arg {}", 42 as u32,);
    Ok(())
}
