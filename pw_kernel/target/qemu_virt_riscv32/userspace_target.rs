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

use console_backend as _;
use kernel as _;

use target_common::{declare_target, TargetInterface};

const STACK_SIZE_BYTES: usize = 2048;

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "QEMU-VIRT-RISCV Userspace Demo";

    fn main() -> ! {
        // TODO: davidroth - codegen process/thread creation
        let start_fn_one = unsafe { core::mem::transmute::<usize, fn(usize)>(0x80100000_usize) };
        let process_one = kernel::init_non_priv_process!("process one");
        let thread_one_main =
            kernel::init_non_priv_thread!("main one", process_one, start_fn_one, STACK_SIZE_BYTES);

        let start_fn_two = unsafe { core::mem::transmute::<usize, fn(usize)>(0x80200000_usize) };
        let process_two = kernel::init_non_priv_process!("process two");
        let thread_two_main =
            kernel::init_non_priv_thread!("main two", process_two, start_fn_two, STACK_SIZE_BYTES);

        kernel::start_thread(thread_one_main);
        kernel::start_thread(thread_two_main);

        loop {}
    }
}

declare_target!(Target);

#[riscv_rt::entry]
fn main() -> ! {
    kernel::Kernel::main();
}
