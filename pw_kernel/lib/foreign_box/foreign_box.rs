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

use core::marker::PhantomData;
use core::ops::{Deref, DerefMut};
use core::ptr::NonNull;
use core::sync::atomic::Ordering;

use pw_atomic::AtomicUsize;

macro_rules! local_panic {
    ($msg:literal $(,)?) => {{
        if cfg!(feature = "core_panic") {
            panic!($msg);
        } else {
            pw_assert::panic!($msg);
        }

    }};

    ($msg:literal, $($args:expr),* $(,)?) => {{
        #[allow(clippy::unnecessary_cast)]
        if cfg!(feature = "core_panic") {
            panic!($msg, $($args),*);
        } else {
            pw_assert::panic!($msg, $($args),*);
        }
    }};
}

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

// Safety: Same bounds on `T` as [`Box`].
unsafe impl<T: Send + ?Sized> Send for ForeignBox<T> {}
unsafe impl<T: Sync + ?Sized> Sync for ForeignBox<T> {}

impl<T: ?Sized> ForeignBox<T> {
    #[allow(dead_code)]
    /// Create a new `ForeignBox` from a `NonNull<T>`.
    ///
    /// # Safety
    /// The caller guarantees that `ptr` remains valid throughout the lifetime
    /// of the `ForeignBox` object.
    #[must_use]
    pub const unsafe fn new(ptr: NonNull<T>) -> Self {
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
            local_panic!("Null pointer");
        };
        unsafe { Self::new(ptr) }
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
    /// # Safety
    /// Creates an "mutable unenforceable borrow" of the contained data.
    #[must_use]
    pub unsafe fn as_mut_ptr(&mut self) -> *mut T {
        unsafe { self.inner.as_mut() }
    }
}

impl<T: ?Sized> Drop for ForeignBox<T> {
    fn drop(&mut self) {
        if !self.consumed {
            local_panic!(
                "ForeignBox@{:08x} dropped before being consumed!",
                self.inner.as_ptr().cast::<()>() as usize
            );
        }
    }
}

impl<T: ?Sized> From<&'static mut T> for ForeignBox<T> {
    fn from(t: &'static mut T) -> ForeignBox<T> {
        // SAFETY: The `'static` lifetime guarantees that `t`'s referent will
        // remain valid for the lifetime of the returned `ForeignBox`.
        unsafe { ForeignBox::new(NonNull::from(t)) }
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

pub struct ForeignRcBox<A: AtomicUsize, T: ?Sized> {
    inner: ForeignBox<T>,
    // Counts the number of `ForeignRc`s that reference this Box.  If this
    // number is zero, there are no references.  Will panic if dropped while
    // there are outstanding references.
    ref_count: A,
}

pub struct ForeignRc<A: AtomicUsize, T: ?Sized> {
    rc_box: NonNull<ForeignRcBox<A, T>>,
}

impl<A: AtomicUsize, T: ?Sized> Deref for ForeignRc<A, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        // SAFETY: self.rc_box validity is guaranteed by self.rc_box panicking
        // if it is dropped with a ref_count > 0.
        unsafe { self.rc_box.as_ref().inner.deref() }
    }
}

impl<A: AtomicUsize, T: ?Sized> Clone for ForeignRc<A, T> {
    fn clone(&self) -> Self {
        // SAFETY: self.rc_box validity is guaranteed by self.rc_box panicking
        // if it is dropped with a ref_count > 0.  Since a new `ForeignRc` is
        // being created, the box's ref_count is incremented.
        unsafe {
            self.rc_box
                .as_ref()
                .ref_count
                .fetch_add(1, Ordering::SeqCst)
        };
        Self {
            rc_box: self.rc_box,
        }
    }
}

impl<A: AtomicUsize, T: ?Sized> Drop for ForeignRc<A, T> {
    fn drop(&mut self) {
        // SAFETY: self.rc_box's validity is guarding by self.rc_box panicking
        // if it is dropped with a ref_count > 0.  Since this ForeignRc is being
        // dropped, the box's refcount is decremented.
        unsafe {
            self.rc_box
                .as_ref()
                .ref_count
                .fetch_sub(1, Ordering::SeqCst)
        };
    }
}

impl<A: AtomicUsize, T: ?Sized> ForeignRcBox<A, T> {
    /// # Safety
    /// This method has the same safety preconditions as
    /// [`ForeignBox::new`].
    #[must_use]
    pub unsafe fn new(ptr: NonNull<T>) -> Self {
        Self {
            inner: unsafe { ForeignBox::new(ptr) },
            ref_count: A::new(0),
        }
    }
}

impl<A: AtomicUsize, T: ?Sized> ForeignRcBox<A, T> {
    pub fn get_ref(&self) -> ForeignRc<A, T> {
        self.ref_count.fetch_add(1, Ordering::SeqCst);
        ForeignRc {
            rc_box: NonNull::from_ref(self),
        }
    }

    /// Consumes the `ForeignRcBox`, returning the inner `NonNull` T.
    ///
    /// # Panics
    /// Panics if there are any outstanding `ForeignRc` references.
    #[allow(clippy::must_use_candidate)]
    pub fn consume(self) -> NonNull<T> {
        if self.ref_count.load(Ordering::SeqCst) != 0 {
            local_panic!(
                "ForeignRcBox@{:08x} consumed with outstanding references",
                self.inner.inner.as_ptr().cast::<()>().expose_provenance() as usize
            );
        }
        let this = core::mem::ManuallyDrop::new(self);
        // SAFETY: We are taking ownership of the `inner` field, and because
        // `this` is a `ManuallyDrop`, a double-free is not possible.
        let inner = unsafe { core::ptr::read(&this.inner) };
        inner.consume()
    }
}

impl<A: AtomicUsize, T: ?Sized> Drop for ForeignRcBox<A, T> {
    fn drop(&mut self) {
        if self.ref_count.load(Ordering::SeqCst) != 0 {
            #[cfg(test)]
            {
                // Mark the inner ForeignBox as consumed to avoid a double panic
                // when testing for panics using the standard Rust test harness.
                // The double panic occurs because, when unwinding the stack on
                // panic, the inner foreign box is still dropped.
                self.inner.consumed = true;
            }

            local_panic!(
                "ForeignRcBox@{:08x} dropped with outstanding references",
                self.inner.inner.as_ptr().cast::<()>().expose_provenance() as usize
            );
        }
    }
}

#[cfg(test)]
mod tests {
    // Ensure that the console backend (needed for pw_log) is linked.
    use console_backend as _;

    use super::*;
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

    #[test]
    fn rc_box_ref_count_is_correct() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let rc_box = unsafe {
            ForeignRcBox::<core::sync::atomic::AtomicUsize, _>::new(NonNull::new_unchecked(ptr))
        };

        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 0);

        let r1 = rc_box.get_ref();
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 1);

        let r2 = rc_box.get_ref();
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 2);

        drop(r1);
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 1);

        drop(r2);
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 0);

        rc_box.consume();
    }

    #[test]
    fn rc_box_can_be_cloned() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let rc_box = unsafe {
            ForeignRcBox::<core::sync::atomic::AtomicUsize, _>::new(NonNull::new_unchecked(ptr))
        };

        let r1 = rc_box.get_ref();
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 1);

        let r2 = r1.clone();
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 2);

        drop(r1);
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 1);

        drop(r2);
        assert_eq!(rc_box.ref_count.load(Ordering::SeqCst), 0);

        rc_box.consume();
    }

    #[test]
    fn rc_box_can_be_read_through_deref() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let rc_box = unsafe {
            ForeignRcBox::<core::sync::atomic::AtomicUsize, _>::new(NonNull::new_unchecked(ptr))
        };

        let rc = rc_box.get_ref();
        assert_eq!(*rc, 0xdecafbad_u32);
        drop(rc);

        rc_box.consume();
    }

    #[should_panic]
    #[test]
    fn rc_box_panics_when_dropped_with_no_outstanding_refs() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let _rc_box = unsafe {
            ForeignRcBox::<core::sync::atomic::AtomicUsize, _>::new(NonNull::new_unchecked(ptr))
        };

        // This should panic when rc_box is dropped.
    }

    #[should_panic]
    #[test]
    fn rc_box_panics_when_dropped_with_outstanding_refs() {
        let mut value = 0xdecafbad_u32;
        let ptr = &raw mut value;
        let rc_box = unsafe {
            ForeignRcBox::<core::sync::atomic::AtomicUsize, _>::new(NonNull::new_unchecked(ptr))
        };

        let _rc = rc_box.get_ref();

        // This should panic when rc_box is dropped.
        drop(rc_box);
    }
}
