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

#[cfg(not(feature = "user_space"))]
pub(crate) use arm_cortex_m_macro::kernel_only_exception as exception;
#[cfg(feature = "user_space")]
pub(crate) use arm_cortex_m_macro::user_space_exception as exception;

use pw_log::info;

/// Exception frame with all registers
#[repr(C)]
pub struct FullExceptionFrame {
    pub kernel: KernelExceptionFrame,
    pub user: ExceptionFrame,
}

/// Exception frame with only the registers pushed by the core.
#[repr(C)]
pub struct ExceptionFrame {
    pub r0: u32,
    pub r1: u32,
    pub r2: u32,
    pub r3: u32,
    pub r12: u32,
    pub lr: u32,
    pub pc: u32,
    pub psr: u32,
}

impl ExceptionFrame {
    #[inline(never)]
    fn dump(&self) {
        info!("Exception frame {:#08x}:", &raw const *self as usize);
        info!(
            "r0  {:#010x} r1 {:#010x} r2  {:#010x} r3  {:#010x}",
            self.r0 as u32, self.r1 as u32, self.r2 as u32, self.r3 as u32
        );
    }
}

/// Exception frame with the registers that the kernel first level assembly
/// exception handler wrapper pushes.
#[repr(C)]
pub struct KernelExceptionFrame {
    pub r4: u32,
    pub r5: u32,
    pub r6: u32,
    pub r7: u32,
    pub r8: u32,
    pub r9: u32,
    pub r10: u32,
    pub r11: u32,
    #[cfg(feature = "user_space")]
    pub psp: u32,
    pub return_address: u32,
}

impl KernelExceptionFrame {
    #[inline(never)]
    fn dump(&self) {
        info!("Kernel exception frame {:#08x}:", &raw const *self as usize);
        info!(
            "r4  {:#010x} r5 {:#010x} r6  {:#010x} r7  {:#010x}",
            self.r4 as u32, self.r5 as u32, self.r6 as u32, self.r7 as u32
        );
        info!(
            "r8  {:#010x} r9 {:#010x} r10 {:#010x} r11 {:#010x}",
            self.r8 as u32, self.r9 as u32, self.r10 as u32, self.r11 as u32
        );
    }
}

#[inline(never)]
fn dump_exception_frame(frame: &FullExceptionFrame) {
    frame.user.dump();
    frame.kernel.dump();
}

#[exception(exception = "HardFault")]
#[no_mangle]
extern "C" fn pw_kernel_hard_fault(frame: *mut FullExceptionFrame) -> ! {
    info!("HardFault");
    dump_exception_frame(unsafe { &*frame });
    #[allow(clippy::empty_loop)]
    loop {}
}

#[exception(exception = "DefaultHandler")]
#[no_mangle]
extern "C" fn pw_kernel_default(frame: *mut FullExceptionFrame) -> ! {
    info!("DefaultHandler");
    dump_exception_frame(unsafe { &*frame });
    #[allow(clippy::empty_loop)]
    loop {}
}

#[exception(exception = "NonMaskableInt")]
#[no_mangle]
extern "C" fn pw_kernel_non_maskable_int(frame: *mut FullExceptionFrame) -> ! {
    info!("NonMaskableInt");
    dump_exception_frame(unsafe { &*frame });
    #[allow(clippy::empty_loop)]
    loop {}
}

#[exception(exception = "MemoryManagement")]
#[no_mangle]
extern "C" fn pw_kernel_memory_management(frame: *mut FullExceptionFrame) -> ! {
    let mmfar = 0xE000ED34 as *const u32;
    info!(
        "MemoryManagement exception at {:08x}",
        unsafe { mmfar.read_volatile() } as u32
    );
    dump_exception_frame(unsafe { &*frame });

    #[allow(clippy::empty_loop)]
    loop {}
}

#[exception(exception = "BusFault")]
#[no_mangle]
extern "C" fn pw_kernel_bus_fault(frame: *mut FullExceptionFrame) -> ! {
    let bfar = 0xE000ED38 as *const u32;
    info!(
        "BusFault exception at {:08x}",
        unsafe { bfar.read_volatile() } as u32
    );
    dump_exception_frame(unsafe { &*frame });
    #[allow(clippy::empty_loop)]
    loop {}
}

#[exception(exception = "UsageFault")]
#[no_mangle]
extern "C" fn pw_kernel_usage_fault(frame: *mut FullExceptionFrame) -> ! {
    info!("UsageFault");
    dump_exception_frame(unsafe { &*frame });
    #[allow(clippy::empty_loop)]
    loop {}
}

// PendSV is defined in thread.rs
// SVCall is defined in syscall.rs

#[exception(exception = "DebugMonitor")]
#[no_mangle]
extern "C" fn pw_kernel_debug_monitor(frame: *mut FullExceptionFrame) -> ! {
    info!("DebugMonitor");
    dump_exception_frame(unsafe { &*frame });
    #[allow(clippy::empty_loop)]
    loop {}
}
