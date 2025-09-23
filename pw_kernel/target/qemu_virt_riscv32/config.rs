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

use core::ops::Range;

pub use kernel_config::{
    ClintTimerConfigInterface, ExceptionMode, InterruptHandler, InterruptTable,
    InterruptTableEntry, KernelConfigInterface, PlicConfigInterface, RiscVKernelConfigInterface,
};

pub struct KernelConfig;

impl KernelConfigInterface for KernelConfig {
    const SYSTEM_CLOCK_HZ: u64 = 10_000_000;
}

impl RiscVKernelConfigInterface for KernelConfig {
    type Timer = TimerConfig;
    const MTIME_HZ: u64 = KernelConfig::SYSTEM_CLOCK_HZ;
    const PMP_ENTRIES: usize = 16;
    const PMP_USERSPACE_ENTRIES: Range<usize> = Range {
        start: 0usize,
        end: Self::PMP_ENTRIES,
    };
    const PMP_GRANULARITY: usize = 0;

    fn get_exception_mode() -> ExceptionMode {
        ExceptionMode::Direct
    }
}

pub struct PlicConfig;

unsafe extern "Rust" {
    static INTERRUPT_TABLE: [InterruptTableEntry; PlicConfig::INTERRUPT_TABLE_SIZE];
}

impl PlicConfigInterface for PlicConfig {
    const PLIC_BASE_ADDRESS: usize = 0x0c000000;

    const NUM_IRQS: u32 = 128;

    // 11 as the UART IRQ 10 is the highest handled interrupt.
    const INTERRUPT_TABLE_SIZE: usize = 11;

    fn interrupt_table() -> &'static InterruptTable {
        unsafe { &INTERRUPT_TABLE }
    }
}

pub struct TimerConfig;

const TIMER_BASE: usize = 0x200_0000;

impl ClintTimerConfigInterface for TimerConfig {
    const MTIME_REGISTER: usize = TIMER_BASE + 0xbff8;
    const MTIMECMP_REGISTER: usize = TIMER_BASE + 0x4000;
}
