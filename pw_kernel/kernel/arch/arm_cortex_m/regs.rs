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
#![allow(dead_code)]

pub mod mpu;
pub mod nvic;
pub mod scb;
pub mod systick;

pub use mpu::Mpu;
pub use nvic::Nvic;
pub use scb::Scb;
pub use systick::SysTick;

pub struct Regs {
    pub mpu: Mpu,
    pub nvic: Nvic,
    pub systick: SysTick,
    pub scb: Scb,
}

impl Regs {
    pub const fn get() -> Self {
        Regs {
            mpu: Mpu::new(),
            nvic: Nvic::new(),
            systick: SysTick::new(),
            scb: Scb::new(),
        }
    }
}
