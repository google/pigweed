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

pub trait RO<T> {
    const ADDR: usize;

    /// Read a raw value from the register
    ///
    /// # Safety
    /// The caller must guarantee that provided `ADDR` is accessible.
    #[inline]
    unsafe fn raw_read(&self) -> T {
        (Self::ADDR as *const T).read_volatile()
    }
}

pub trait RW<T> {
    const ADDR: usize;

    /// Read a raw value from the register
    ///
    /// # Safety
    /// The caller must guarantee that provided `ADDR` is accessible.
    #[inline]
    unsafe fn raw_read(&self) -> T {
        (Self::ADDR as *const T).read_volatile()
    }

    /// Write a raw value to the specified register
    ///
    /// # Safety
    /// The caller must guarantee that the value is valid for the provided `ADDR`
    /// and the `ADDR` is accessible.
    #[inline]
    unsafe fn raw_write(&mut self, val: T) {
        (Self::ADDR as *mut T).write_volatile(val)
    }
}

#[macro_export]
macro_rules! ro_bool_field {
    ($val_type:ty, $name:ident, $offset:literal) => {
        #[inline]
        pub const fn $name(&self) -> bool {
            $crate::ops::get_bool(self.0 as usize, $offset)
        }
    };
}

#[macro_export]
macro_rules! rw_bool_field {
    ($val_type:ty, $name:ident, $offset:literal) => {
        ro_bool_field!($val_type, $name, $offset);
        paste::paste! {
          #[inline]
          pub const fn [<with_ $name>](self, val: bool) -> Self {
              Self($crate::ops::set_bool(self.0 as usize, $offset, val) as $val_type)
          }
        }
    };
}

#[macro_export]
macro_rules! ro_int_field {
    ($val_type:ty, $name:ident, $start:literal, $end:literal, $ty:ty) => {
        #[inline]
        pub const fn $name(&self) -> $ty {
            $crate::ops::get_usize(self.0 as usize, $start, $end) as $ty
        }
    };
}

#[macro_export]
macro_rules! rw_int_field {
    ($val_type:ty, $name:ident, $start:literal, $end:literal, $ty:ty) => {
        ro_int_field!($val_type, $name, $start, $end, $ty);
        paste::paste! {
          #[inline]
          pub const fn [<with_ $name>](self, val: $ty) -> Self {
              Self($crate::ops::set_usize(self.0 as usize, $start, $end, val as usize) as $val_type)
          }
        }
    };
}

#[macro_export]
macro_rules! ro_masked_field {
    ($name:ident, $mask:expr, $ty:ty) => {
        #[inline]
        pub const fn $name(&self) -> $ty {
            self.0 & $mask
        }
    };
}

#[macro_export]
macro_rules! rw_masked_field {
    ($name:ident, $mask:expr, $ty:ty) => {
        ro_masked_field!($name, $mask, $ty);

        paste::paste! {
            #[inline]
            pub const fn [<with_ $name>](self, val: $ty) -> Self {
              Self(self.0 & !$mask | (val & $mask))
            }
        }
    };
}

#[macro_export]
macro_rules! ro_reg {
    ($name:ident, $val_type:ident, $addr:literal) => {
        pub struct $name;
        impl RO<u32> for $name {
            const ADDR: usize = $addr;
        }
        impl $name {
            #[inline]
            pub fn read(&self) -> $val_type {
                $val_type(unsafe { self.raw_read() })
            }
        }
    };
}

#[macro_export]
macro_rules! rw_reg {
    ($name:ident, $val_type:ident, $addr:literal) => {
        pub struct $name;
        impl RW<u32> for $name {
            const ADDR: usize = $addr;
        }
        impl $name {
            #[inline]
            pub fn read(&self) -> $val_type {
                $val_type(unsafe { self.raw_read() })
            }

            #[inline]
            pub fn write(&mut self, val: $val_type) {
                unsafe { self.raw_write(val.0) }
            }
        }
    };
}

pub mod ops {
    #[inline]
    pub const fn mask(start: usize, end: usize) -> usize {
        let length = end - start + 1;
        if length == usize::BITS as usize {
            // Special case full mask to keep shifting logic below from overflowing.
            usize::MAX
        } else {
            ((1usize << length) - 1) << start
        }
    }

    #[inline]
    pub const fn mask_u32(start: u32, end: u32) -> u32 {
        let length = end - start + 1;
        if length == u32::BITS {
            // Special case full mask to keep shifting logic below from overflowing.
            u32::MAX
        } else {
            ((1u32 << length) - 1) << start
        }
    }

    #[inline]
    pub const fn get_bool(value: usize, bit: usize) -> bool {
        (value >> bit) & 0x1 == 0x1
    }

    #[inline]
    pub const fn set_bool(value: usize, bit: usize, field_value: bool) -> usize {
        value & !(1 << bit) | ((field_value as usize) << bit)
    }

    #[inline]
    pub const fn get_u32(value: u32, start: u32, end: u32) -> u32 {
        (value & mask_u32(start, end)) >> start
    }

    #[inline]
    pub const fn set_u32(value: u32, start: u32, end: u32, field_value: u32) -> u32 {
        let mask = mask_u32(start, end);
        (value & !mask) | ((field_value << start) & mask)
    }

    #[inline]
    pub const fn get_usize(value: usize, start: usize, end: usize) -> usize {
        (value & mask(start, end)) >> start
    }

    #[inline]
    pub const fn set_usize(value: usize, start: usize, end: usize, field_value: usize) -> usize {
        let mask = mask(start, end);
        (value & !mask) | ((field_value << start) & mask)
    }
}
