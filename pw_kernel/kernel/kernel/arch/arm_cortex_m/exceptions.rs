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
#![allow(non_snake_case)]

//use core::sync::atomic::{self, Ordering};
use core::arch::naked_asm;
use pw_log::info;

#[repr(C)]
pub struct FullExceptionFrame {
    // Extra state pushed by the first level assembly handler
    pub r4: u32,
    pub r5: u32,
    pub r6: u32,
    pub r7: u32,
    pub r8: u32,
    pub r9: u32,
    pub r10: u32,
    pub r11: u32,
    pub return_address: u32,

    // State that hardware pushes automatically
    pub r0: u32,
    pub r1: u32,
    pub r2: u32,
    pub r3: u32,
    pub r12: u32,
    pub lr: u32,
    pub pc: u32,
    pub psr: u32,
}

#[inline(never)]
fn dump_exception_frame(frame: *const FullExceptionFrame) {
    unsafe {
        info!("Exception frame {:#08x}:", frame as usize);
        info!(
            "r0  {:#010x} r1 {:#010x} r2  {:#010x} r3  {:#010x}",
            (*frame).r0,
            (*frame).r1,
            (*frame).r2,
            (*frame).r3
        );
        info!(
            "r4  {:#010x} r5 {:#010x} r6  {:#010x} r7  {:#010x}",
            (*frame).r4,
            (*frame).r5,
            (*frame).r6,
            (*frame).r7
        );
        info!(
            "r8  {:#010x} r9 {:#010x} r10 {:#010x} r11 {:#010x}",
            (*frame).r8,
            (*frame).r9,
            (*frame).r10,
            (*frame).r11
        );
        info!(
            "r12 {:#010x} lr {:#010x} pc  {:#010x} xpsr {:#010x}",
            (*frame).r12,
            (*frame).lr,
            (*frame).pc,
            (*frame).psr
        );
    }
}

// The real hard fault handler.
// TODO: figure out how to make a macro of this trampoline to share between handlers.
#[no_mangle]
#[naked]
pub unsafe extern "C" fn HardFault() -> ! {
    unsafe {
        naked_asm!(
            "
            push    {{ r4 - r11, lr }}  // save the additional registers
            mov     r0, sp
            sub     sp, 4               // realign the stack to 8 byte boundary
            bl      _HardFault
            mov     sp, r0
            pop     {{ r4 - r11, pc }}

        "
        )
    }
}

#[no_mangle]
pub unsafe extern "C" fn _HardFault(frame: *mut FullExceptionFrame) -> ! {
    info!("HardFault");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn DefaultHandler() -> ! {
    info!("DefaultHandler");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn NonMaskableInt() -> ! {
    info!("NonMaskableInt");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn MemoryManagement() -> ! {
    info!("MemoryManagement");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn BusFault() -> ! {
    info!("BusFault");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn UsageFault() -> ! {
    info!("UsageFault");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn SVCall() -> ! {
    info!("SVCall");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn DebugMonitor() -> ! {
    info!("DebugMonitor");
    #[allow(clippy::empty_loop)]
    loop {}
}
