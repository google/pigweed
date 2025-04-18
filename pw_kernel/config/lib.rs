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

// Kernel configuration common to all architectures.
pub trait KernelConfigInterface {}

// Cortex-M specific configuration.
// TODO: davidroth - Once Arch is out of tree, move this configuration also.
pub trait CortexMKernelConfigInterface {}

// RISC-V specific configuration.
// TODO: davidroth - Once Arch is out of tree, move this configuration also.
pub trait RiscVKernelConfigInterface {}
