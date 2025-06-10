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

// This module uses the std test harness to allow catching of panics.
#![cfg_attr(not(test), no_std)]

use core::{
    marker::PhantomData,
    ops::{Deref, DerefMut},
    ptr::NonNull,
};

pub struct ForeignBox<T: ?Sized> {
    inner: NonNull<T>,

    // Guards against the type being dropped w/o a consume call.
    //
    // TODO: add feature to disable this field on production builds to avoid
    // the overhead of the extra bool.
    consumed: bool,

    // NunNull is covariant over `T` which can not be guaranteed of `KernelRef`.
    // Use `PhantomData` in mark as invariant.
    _phantom: PhantomData<T>,
}

impl<T: ?Sized> ForeignBox<T> {
    #[allow(dead_code)]
    /// Create a new `ForeignBox` from a `NonNull<T>`.
    ///
    /// # Safety
    /// The caller guarantees that `ptr` remains valid throughout the lifetime
    /// of the `ForeignBox` object.
    #[must_use]
    pub unsafe fn new(ptr: NonNull<T>) -> Self {
        Self {
            inner: ptr,
            consumed: false,
            _phantom: PhantomData,
        }
    }

    /// Create a new `ForeignBox` from a `*mut T`.
    ///
    /// # Panics
    /// panics if `ptr` is null.
    /// # Safety
    /// The caller guarantees that `ptr` remains valid throughout the lifetime
    /// of the `ForeignBox` object.
    #[must_use]
    pub unsafe fn new_from_ptr(ptr: *mut T) -> Self {
        let Some(ptr) = NonNull::new(ptr) else {
            if cfg!(feature = "core_panic") {
                panic!("Null pointer");
            } else {
                pw_assert::panic!("Null pointer");
            }
        };
        Self::new(ptr)
    }

    #[allow(clippy::must_use_candidate)]
    pub fn consume(mut self) -> NonNull<T> {
        self.consumed = true;
        self.inner
    }

    /// Returns a pointer to the contained data.
    ///
    /// # Safety
    /// Creates an "unenforceable borrow" of the contained data.
    #[must_use]
    pub unsafe fn as_ptr(&self) -> *const T {
        self.inner.as_ptr()
    }

    /// Returns a mutable pointer to the contained data.
    ///
    ///  # Safety
    /// Creates an "mutable unenforceable borrow" of the contained data.
    #[must_use]
    pub unsafe fn as_mut_ptr(&mut self) -> *mut T {
        self.inner.as_mut()
    }
}

impl<T: ?Sized> Drop for ForeignBox<T> {
    fn drop(&mut self) {
        if !self.consumed {
            if cfg!(feature = "core_panic") {
                panic!(
                    "ForeignBox@{:08x} dropped before being consumed!",
                    self.inner.as_ptr().cast::<()>().expose_provenance()
                );
            } else {
                pw_assert::panic!(
                    "ForeignBox@{:08x} dropped before being consumed!",
                    self.inner.as_ptr().cast::<()>() as usize
                );
            }
        }
    }
}

impl<T: ?Sized> AsRef<T> for ForeignBox<T> {
    fn as_ref(&self) -> &T {
        unsafe { self.inner.as_ref() }
    }
}

impl<T: ?Sized> AsMut<T> for ForeignBox<T> {
    fn as_mut(&mut self) -> &mut T {
        unsafe { self.inner.as_mut() }
    }
}

impl<T: ?Sized> Deref for ForeignBox<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.as_ref()
    }
}

impl<T: ?Sized> DerefMut for ForeignBox<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Ensure that the console backend (needed for pw_log) is linked.
    use console_backend as _;
    #[test]
    fn consume_returns_the_same_pointer() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let foreign_box = unsafe { ForeignBox::new(NonNull::new_unchecked(ptr)) };
        let consumed_ptr = foreign_box.consume();

        assert_eq!(ptr, consumed_ptr.as_ptr());
    }

    #[should_panic]
    #[test]
    fn non_consumed_box_panics_when_dropped() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let _foreign_box = unsafe { ForeignBox::new(NonNull::new_unchecked(ptr)) };

        // Should panic when foreign_box is dropped as it goes out of scope.
    }

    #[test]
    fn value_can_be_read_through_as_ref() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let foreign_box = unsafe { ForeignBox::new(NonNull::new_unchecked(ptr)) };
        assert_eq!(value, *(foreign_box.as_ref()));

        // Prevent foreign_box from being dropped.
        let _ = foreign_box.consume();
    }

    #[test]
    fn value_can_be_modified_through_as_mut() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let mut foreign_box = unsafe { ForeignBox::new(NonNull::new_unchecked(ptr)) };
        *(foreign_box.as_mut()) = 0xcafecafe;

        assert_eq!(unsafe { ptr.read_volatile() }, 0xcafecafe);

        // Prevent foreign_box from being dropped.
        let _ = foreign_box.consume();
    }

    #[test]
    fn value_can_be_read_through_deref() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let foreign_box = unsafe { ForeignBox::new(NonNull::new_unchecked(ptr)) };
        assert_eq!(value, *foreign_box);

        // Prevent foreign_box from being dropped.
        let _ = foreign_box.consume();
    }

    #[test]
    fn value_can_be_modified_through_deref_mut() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let mut foreign_box = unsafe { ForeignBox::new(NonNull::new_unchecked(ptr)) };
        *foreign_box = 0xcafecafe;

        assert_eq!(unsafe { ptr.read_volatile() }, 0xcafecafe);

        // Prevent foreign_box from being dropped.
        let _ = foreign_box.consume();
    }

    trait TestDynTrait {
        fn number(&self) -> u32;
    }

    struct TimesOne {
        val: u32,
    }

    impl TestDynTrait for TimesOne {
        fn number(&self) -> u32 {
            self.val
        }
    }

    #[allow(dead_code)]
    struct TimesTwo {
        val: u32,
    }

    #[allow(dead_code)]
    impl TestDynTrait for TimesTwo {
        fn number(&self) -> u32 {
            self.val * 2
        }
    }

    fn get_number(val: &ForeignBox<dyn TestDynTrait>) -> u32 {
        val.as_ref().number()
    }

    #[test]
    fn supports_dynamic_dispatch() {
        let mut times_one_val = TimesOne { val: 10 };
        let ptr = &raw mut times_one_val;
        let times_one_box = unsafe { ForeignBox::<dyn TestDynTrait>::new_from_ptr(ptr) };

        let mut times_two_val = TimesTwo { val: 10 };
        let ptr = &raw mut times_two_val;
        let times_two_box = unsafe { ForeignBox::<dyn TestDynTrait>::new_from_ptr(ptr) };

        assert_eq!(get_number(&times_one_box), 10);
        assert_eq!(get_number(&times_two_box), 20);

        times_one_box.consume();
        times_two_box.consume();
    }
}
