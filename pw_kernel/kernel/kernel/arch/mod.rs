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

#[cfg(feature = "arch_arm_cortex_m")]
mod arm_cortex_m;
#[cfg(feature = "arch_arm_cortex_m")]
pub use arm_cortex_m::Arch;

#[cfg(feature = "arch_host")]
mod host;
#[cfg(feature = "arch_host")]
pub use host::Arch;

#[const_trait]
pub trait ThreadState {
    #[allow(dead_code)]
    fn new() -> Self;
}

pub trait ArchInterface {
    type ThreadState: const ThreadState;

    fn early_init() {}
    fn init() {}

    // fill in more arch implementation functions from the kernel here:
    // TODO: interrupt management
    //       context switching, thread creation, management
    //       arch-specific backtracing
}
