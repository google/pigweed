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

/// Like [`From`], but supporting target-specific casts.
///
/// [`From`] does not support casts which are not valid on all targets (such as
/// `u32` to `usize`, which is not valid on 16-bit platforms). `CastFrom`, by
/// contrast, supports all conversions which are valid on a given target. As a
/// consequence, uses of `CastFrom` may not be portable to all targets.
///
/// Like [`From`], `CastFrom` only supports lossless casts.
pub trait CastFrom<T> {
    fn cast_from(t: T) -> Self;
}

/// Like [`Into`], but supporting target-specific casts.
///
/// See [`CastFrom`]'s docs for more information.
///
/// Like [`Into`], `CastInto` only supports lossless casts.
pub trait CastInto<T> {
    fn cast_into(self) -> T;
}

impl<T, U: CastFrom<T>> CastInto<U> for T {
    fn cast_into(self) -> U {
        U::cast_from(self)
    }
}

/// Performs a lossless cast.
///
/// `cast!` performs the same cast as [`CastFrom::cast_from`] or
/// [`CastInto::cast_into`], but unlike those methods, it works in a `const`
/// context.
#[macro_export]
macro_rules! cast {
    ($e:expr) => { $crate::cast!($e => _) };
    ($e:expr => $t:ty) => {{
        #[allow(clippy::as_underscore)]
        if false {
            // SAFETY: This branch is never taken.
            #[allow(clippy::macro_metavars_in_unsafe)]
            unsafe { $crate::__cast($e) }
        } else {
            // The preceding call to `__cast` ensures that the source type (the
            // type of `$e`) can be casted into the destination type (`$t`) via
            // `CastFrom`, which ensures that this is a lossless cast.
            $e as $t
        }
    }};
}

/// Attempts to perform a lossless cast.
///
/// This macro evaluates to an expression of type `Option<$t>`. It is similar to
/// [`TryInto`], but it works in a `const` context.
#[macro_export]
macro_rules! try_cast {
    ($e:expr => $t:ty) => {{
        let min = $crate::cast!(<$t>::MIN);
        let max = $crate::cast!(<$t>::MAX);
        // Triggered when `$e` is a literal which trivially satisfies one of
        // these comparisons.
        #[allow(unused_comparisons)]
        // Triggered when invoked with `x + 1` or `y - 1`.
        #[allow(clippy::int_plus_one)]
        if $e >= min && $e <= max {
            #[allow(clippy::cast_possible_truncation)]
            Some($e as $t)
        } else {
            None
        }
    }};
}

// A test to ensure that `cast!` and `try_cast!` work in a const context, and
// work properly.
const _: () = {
    let x: usize = cast!(0u32);
    assert!(x == 0usize);
    assert!(cast!(0u32 => usize) == 0usize);

    assert!(try_cast!(0u32 => u16).unwrap() == 0u16);
    assert!(try_cast!(256u16 => u8).is_none());

    #[allow(clippy::identity_op)]
    {
        // Test more complex `$e:expr` fragments.
        let x: usize = cast!(0u32 + 1);
        assert!(x == 1usize);
        assert!(cast!(0u32 + 1 => usize) == 1usize);
        assert!(try_cast!(0u32 + 1 => u16).unwrap() == 1u16);
        assert!(try_cast!(255u16 + 1 => u8).is_none());
    }
};

/// # Safety
///
/// It is UB to call this function.
#[doc(hidden)]
pub const unsafe fn __cast<T, U: CastFrom<T>>(_t: T) -> U {
    unsafe { core::hint::unreachable_unchecked() }
}

macro_rules! impl_cast_from {
    ($($src:ty => $dst:ty),* $(,)?) => {
        $(
            impl CastFrom<$src> for $dst {
                #[allow(clippy::cast_lossless, clippy::cast_possible_truncation)]
                fn cast_from(t: $src) -> $dst {
                    const _: () = assert!(size_of::<$src>() <= size_of::<$dst>());

                    // So long as `$src` and `$dst` are numeric primitives, the
                    // preceding assert ensures that this is a lossless cast.
                    t as $dst
                }
            }
        )*
    };
}

#[cfg(target_pointer_width = "16")]
impl_cast_from!(
    u8 => usize,
    u8 => u16,
    bool => usize,
    u16 => usize,
    usize => u16,
    usize => u32,
    usize => u64,
);

#[cfg(target_pointer_width = "32")]
impl_cast_from!(
    u8 => usize,
    u8 => u16,
    bool => usize,
    u16 => usize,
    u16 => u32,
    u32 => usize,
    usize => u32,
    usize => u64,
);

#[cfg(target_pointer_width = "64")]
impl_cast_from!(
    u8 => usize,
    u8 => u16,
    bool => usize,
    u16 => usize,
    u16 => u32,
    u32 => usize,
    u64 => usize,
    usize => u64,
);
