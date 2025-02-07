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
use cortex_m_rt::ExceptionFrame;
use pw_log::info;

fn dump_exception_frame(frame: &ExceptionFrame) {
    info!("Exception frame:");
    info!(
        "r0 {} r1 {} r2 {} r3 {}",
        frame.r0(),
        frame.r1(),
        frame.r2(),
        frame.r3()
    );
    info!(
        "r12 {} lr {} pc {} xpsr {}",
        frame.r12(),
        frame.lr(),
        frame.pc(),
        frame.xpsr()
    );
}

#[no_mangle]
pub unsafe extern "C" fn HardFault(frame: &ExceptionFrame) -> ! {
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

#[no_mangle]
pub unsafe extern "C" fn PendSV() -> ! {
    info!("PendSV");
    #[allow(clippy::empty_loop)]
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn SysTick() -> ! {
    info!("SysTick");
    #[allow(clippy::empty_loop)]
    loop {}
}
