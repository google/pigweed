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
use target_common::{declare_target, TargetInterface};

const STACK_SIZE_BYTES: usize = 2048;

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "MPS2-AN505 Userspace Demo";

    fn main() -> ! {
        // TODO: davidroth - codegen process/thread creation
        let start_fn_one = 0x10600001_usize;
        const MEMORY_CONFIG_ONE: <kernel::Arch as kernel::ArchInterface>::MemoryConfig =
            <kernel::Arch as kernel::ArchInterface>::MemoryConfig::const_new(&[
                kernel::MemoryRegion {
                    ty: kernel::MemoryRegionType::ReadOnlyExecutable,
                    start: 0x10600000,
                    end: 0x10600000 + 255 * 1024,
                },
                kernel::MemoryRegion {
                    ty: kernel::MemoryRegionType::ReadWriteData,
                    start: 0x38020000,
                    end: 0x38020000 + 64 * 1024,
                },
            ]);
        let process_one = kernel::init_non_priv_process!("process one", MEMORY_CONFIG_ONE);
        let thread_one_main = kernel::init_non_priv_thread!(
            "main one",
            process_one,
            start_fn_one,
            0x38020000 + 63 * 1024, // Initial Stack Pointer
            STACK_SIZE_BYTES
        );

        let start_fn_two = 0x10700001_usize;
        const MEMORY_CONFIG_TWO: <kernel::Arch as kernel::ArchInterface>::MemoryConfig =
            <kernel::Arch as kernel::ArchInterface>::MemoryConfig::const_new(&[
                kernel::MemoryRegion {
                    ty: kernel::MemoryRegionType::ReadOnlyExecutable,
                    start: 0x10700000,
                    end: 0x10700000 + 255 * 1024,
                },
                kernel::MemoryRegion {
                    ty: kernel::MemoryRegionType::ReadWriteData,
                    start: 0x38040000,
                    end: 0x38040000 + 64 * 1024,
                },
            ]);
        let process_two = kernel::init_non_priv_process!("process two", MEMORY_CONFIG_TWO);
        let thread_two_main = kernel::init_non_priv_thread!(
            "main two",
            process_two,
            start_fn_two,
            0x38040000 + 63 * 1024, // Initial Stack Pointer
            STACK_SIZE_BYTES
        );

        kernel::start_thread(thread_one_main);
        kernel::start_thread(thread_two_main);

        loop {}
    }
}

declare_target!(Target);

#[cortex_m_rt::entry]
fn main() -> ! {
    kernel::Kernel::main();
}
