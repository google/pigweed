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

pub trait TargetInterface {
    const NAME: &'static str;

    /// Called at the very beginning of the kernel to set up the console.
    fn console_init() {}

    /// Called at the end of Kernel::main to invoke any target specific
    /// code.
    fn main() -> !;
}

#[macro_export]
macro_rules! declare_target {
    ($target:ty) => {
        #[no_mangle]
        pub fn pw_kernel_target_name() -> &'static str {
            <$target as $crate::TargetInterface>::NAME
        }

        #[no_mangle]
        pub fn pw_kernel_target_console_init() {
            <$target as $crate::TargetInterface>::console_init();
        }

        #[no_mangle]
        pub fn pw_kernel_target_main() -> ! {
            <$target as $crate::TargetInterface>::main();
        }
    };
}

/// Run all ELF static constructors.
///
/// # Safety
///
/// Only call this function if the given architecture does not automatically run
/// ELF constructors.
pub unsafe fn run_ctors() {
    type CtorFn = unsafe extern "C" fn() -> usize;

    extern "C" {
        static __init_array_start: CtorFn;
        static __init_array_end: CtorFn;
    }

    unsafe {
        let start_ptr: *const CtorFn = &__init_array_start;
        let end_ptr: *const CtorFn = &__init_array_end;
        let num_ctors = end_ptr.offset_from(start_ptr).cast_unsigned();
        let ctors = core::slice::from_raw_parts(start_ptr, num_ctors);
        for ctor in ctors {
            let _ = ctor();
        }
    }
}
