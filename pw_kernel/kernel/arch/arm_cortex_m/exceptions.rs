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
            (*frame).r0 as u32,
            (*frame).r1 as u32,
            (*frame).r2 as u32,
            (*frame).r3 as u32
        );
        info!(
            "r4  {:#010x} r5 {:#010x} r6  {:#010x} r7  {:#010x}",
            (*frame).r4 as u32,
            (*frame).r5 as u32,
            (*frame).r6 as u32,
            (*frame).r7 as u32
        );
        info!(
            "r8  {:#010x} r9 {:#010x} r10 {:#010x} r11 {:#010x}",
            (*frame).r8 as u32,
            (*frame).r9 as u32,
            (*frame).r10 as u32,
            (*frame).r11 as u32
        );
        info!(
            "r12 {:#010x} lr {:#010x} pc  {:#010x} xpsr {:#010x}",
            (*frame).r12 as u32,
            (*frame).lr as u32,
            (*frame).pc as u32,
            (*frame).psr as u32
        );
    }
}

macro_rules! exception_trampoline {
    ($exception:ident, $handler:ident) => {
        #[no_mangle]
        #[naked]
        pub unsafe extern "C" fn $exception() -> ! {
            unsafe {
                naked_asm!(concat!(
                    "
            push    {{ r4 - r11, lr }}  // save the additional registers
            mov     r0, sp
            sub     sp, 4               // realign the stack to 8 byte boundary
            bl      ",
                    stringify!($handler),
                    "
            mov     sp, r0
            pop     {{ r4 - r11, pc }}

        "
                ))
            }
        }
    };
}

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_hard_fault(frame: *mut FullExceptionFrame) -> ! {
    info!("HardFault");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(HardFault, pw_kernel_hard_fault);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_default(frame: *mut FullExceptionFrame) -> ! {
    info!("DefaultHandler");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(DefaultHandler, pw_kernel_default);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_non_maskable_int(frame: *mut FullExceptionFrame) -> ! {
    info!("NonMaskableInt");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(NonMaskableInt, pw_kernel_non_maskable_int);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_memory_management(frame: *mut FullExceptionFrame) -> ! {
    let mmfar = 0xE000ED34 as *const u32;
    info!(
        "MemoryManagement exception at {:08x}",
        mmfar.read_volatile() as u32
    );
    dump_exception_frame(frame);

    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(MemoryManagement, pw_kernel_memory_management);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_bus_fault(frame: *mut FullExceptionFrame) -> ! {
    let bfar = 0xE000ED38 as *const u32;
    info!("BusFault exception at {:08x}", bfar.read_volatile() as u32);
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(BusFault, pw_kernel_bus_fault);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_usage_fault(frame: *mut FullExceptionFrame) -> ! {
    info!("UsageFault");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(UsageFault, pw_kernel_usage_fault);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_sv_call(frame: *mut FullExceptionFrame) -> ! {
    info!("SVCall");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
exception_trampoline!(SVCall, pw_kernel_sv_call);

#[no_mangle]
pub unsafe extern "C" fn pw_kernel_debug_monitor(frame: *mut FullExceptionFrame) -> ! {
    info!("DebugMonitor");
    dump_exception_frame(frame);
    #[allow(clippy::empty_loop)]
    loop {}
}
