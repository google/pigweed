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

use core::cell::UnsafeCell;
use core::sync::atomic::Ordering;

use pw_atomic::{Atomic, AtomicAdd, AtomicLoad, AtomicNew, AtomicStore, AtomicSub, AtomicZero};

// TODO: once the interrupt controller work lands, make use of it here.
unsafe fn disable_interrupts() -> usize {
    let mut val: usize;
    unsafe { core::arch::asm!("csrrw {val}, mstatus, x0", val = out(reg) val) };
    val
}

unsafe fn enabled_interrupts(val: usize) {
    unsafe { core::arch::asm!("csrrw x0, mstatus, {val}", val = in(reg) val) };
}

macro_rules! impl_for_type {
    ($atomic_type:ty, $primitive_type:ty) => {
        impl AtomicAdd<$primitive_type> for $atomic_type {
            fn fetch_add(&self, val: $primitive_type, _ordering: Ordering) -> $primitive_type {
                unsafe {
                    let i = disable_interrupts();
                    let ptr = self.value.get();
                    let current = *ptr;
                    // Wrapping semantics per
                    // https://doc.rust-lang.org/std/sync/atomic/struct.AtomicUsize.html#method.fetch_add
                    *ptr = current.wrapping_add(val);
                    enabled_interrupts(i);
                    current
                }
            }
        }

        impl AtomicSub<$primitive_type> for $atomic_type {
            fn fetch_sub(&self, val: $primitive_type, _ordering: Ordering) -> $primitive_type {
                unsafe {
                    let i = disable_interrupts();
                    let ptr = self.value.get();
                    let current = *ptr;
                    // Wrapping semantics per
                    // https://doc.rust-lang.org/std/sync/atomic/struct.AtomicUsize.html#method.fetch_sub
                    *ptr = current.wrapping_sub(val);
                    enabled_interrupts(i);
                    current
                }
            }
        }

        impl AtomicLoad<$primitive_type> for $atomic_type {
            fn load(&self, _ordering: Ordering) -> $primitive_type {
                unsafe { *self.value.get() }
            }
        }

        impl AtomicStore<$primitive_type> for $atomic_type {
            fn store(&self, val: $primitive_type, _ordering: Ordering) {
                unsafe {
                    *self.value.get() = val;
                }
            }
        }

        impl AtomicNew<$primitive_type> for $atomic_type {
            fn new(val: $primitive_type) -> Self {
                Self {
                    value: UnsafeCell::new(val),
                }
            }
        }

        impl AtomicZero for $atomic_type {
            const ZERO: Self = Self {
                value: UnsafeCell::new(0),
            };
        }

        impl Atomic<$primitive_type> for $atomic_type {}
    };
}

pub struct AtomicUsize {
    value: UnsafeCell<usize>,
}

impl_for_type!(AtomicUsize, usize);
impl pw_atomic::AtomicUsize for AtomicUsize {}

// SAFETY: Atomicity is guaranteed by disabling interrupts on a UP system.
// MP systems are not supported.
unsafe impl Sync for AtomicUsize {}
