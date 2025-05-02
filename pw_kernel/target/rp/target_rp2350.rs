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

use rp235x_hal as hal;

use hal::fugit::RateExtU32;
use hal::uart::{DataBits, StopBits, UartConfig};
use hal::Clock;
use target_common::{declare_target, TargetInterface};

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
        demo::main()
    }
}

declare_target!(Target);

#[cortex_m_rt::entry]
fn main() -> ! {
    // cortex_m_rt does not run ctors, so we do it manually. Note that this is
    // required in order to register tests, which is a prerequisite to calling
    // `run_tests` below.
    unsafe { target_common::run_ctors() };

    #[cfg(not(feature = "test"))]
    kernel::Kernel::main();

    #[cfg(feature = "test")]
    {
        use cortex_m_semihosting::debug::*;
        Target::console_init();
        exit(target_common::run_tests(EXIT_SUCCESS, EXIT_FAILURE));

        // `exit` can return under rare circumstances.
        #[allow(clippy::empty_loop)]
        loop {}
    }
}
