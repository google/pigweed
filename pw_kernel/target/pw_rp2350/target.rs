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

use arch_arm_cortex_m::Arch;
use hal::fugit::RateExtU32;
use hal::uart::{DataBits, StopBits, UartConfig};
use hal::Clock;
#[cfg(test)]
use integration_tests as _;
use target_common::{declare_target, TargetInterface};
use {console_backend as _, kernel as _, rp235x_hal as hal};

#[link_section = ".start_block"]
#[used]
pub static IMAGE_DEF: hal::block::ImageDef = hal::block::ImageDef::secure_exe();

#[link_section = ".bi_entries"]
#[used]
pub static PICOTOOL_ENTRIES: [hal::binary_info::EntryAddr; 5] = [
    hal::binary_info::rp_program_name!(c"maize"),
    hal::binary_info::rp_cargo_version!(),
    hal::binary_info::rp_program_description!(c"Maize"),
    hal::binary_info::rp_program_url!(c"http://pigweed.dev"),
    hal::binary_info::rp_program_build_attribute!(),
];

const XTAL_FREQ_HZ: u32 = 12_000_000u32;

pub struct Target {}

impl TargetInterface for Target {
    const NAME: &'static str = "RP2350";

    fn console_init() {
        let mut pac = hal::pac::Peripherals::take().unwrap();

        let mut watchdog = hal::Watchdog::new(pac.WATCHDOG);

        let clocks = hal::clocks::init_clocks_and_plls(
            XTAL_FREQ_HZ,
            pac.XOSC,
            pac.CLOCKS,
            pac.PLL_SYS,
            pac.PLL_USB,
            &mut pac.RESETS,
            &mut watchdog,
        )
        .unwrap();

        let sio = hal::Sio::new(pac.SIO);

        let pins = hal::gpio::Pins::new(
            pac.IO_BANK0,
            pac.PADS_BANK0,
            sio.gpio_bank0,
            &mut pac.RESETS,
        );

        let uart_pins = (pins.gpio12.into_function(), pins.gpio13.into_function());
        let uart = hal::uart::UartPeripheral::new(pac.UART0, uart_pins, &mut pac.RESETS)
            .enable(
                UartConfig::new(115200.Hz(), DataBits::Eight, None, StopBits::One),
                clocks.peripheral_clock.freq(),
            )
            .unwrap();

        console_backend::register_uart(uart);
    }

    fn main() -> ! {
        // cortex_m_rt does not run ctors, so we do it manually. Note that this
        // is required in order to register tests, which is a prerequisite to
        // calling `run_all_tests` below.
        unsafe { target_common::run_ctors() };

        #[cfg(not(test))]
        {
            static mut DEMO_STATE: demo::DemoState<Arch> = demo::DemoState::new(Arch);
            // SAFETY: `main` is only executed once, so we never generate more
            // than one `&mut` reference to `DEMO_STATE`.
            #[allow(static_mut_refs)]
            demo::main(Arch, unsafe { &mut DEMO_STATE });
        }

        #[cfg(test)]
        {
            use cortex_m_semihosting::debug::*;
            use unittest_core::TestsResult;

            exit(match unittest_core::run_all_tests!() {
                TestsResult::AllPassed => EXIT_SUCCESS,
                TestsResult::SomeFailed => EXIT_FAILURE,
            });

            // `exit` can return under rare circumstances.
            #[allow(unreachable_code, clippy::empty_loop)]
            loop {}
        }
    }
}

declare_target!(Target);

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    use kernel::Arch as _;
    Arch::panic()
}

#[cortex_m_rt::entry]
fn main() -> ! {
    Target::console_init();

    kernel::static_init_state!(static mut INIT_STATE: InitKernelState<Arch>);

    // SAFETY: `main` is only executed once, so we never generate more than one
    // `&mut` reference to `INIT_STATE`.
    #[allow(static_mut_refs)]
    kernel::main(Arch, unsafe { &mut INIT_STATE });
}
