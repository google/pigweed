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

use core::mem::MaybeUninit;

use pw_status::{Error, Result};

/// A fixed-capacity circular buffer.
///
/// The buffer has the following properties:
/// - No restrictions on a power of two on the value of N.
/// - The full size of buffer is used (no wasted space).
/// - When the buffer is full, writing will return an error until the
///   buffer is read from.
///
/// ## Invariants
///
/// For a buffer with `N > 0`, the following invariants are maintained:
///
/// 1. `0 <= self.len <= N`: The number of elements is always between 0 and
///    the capacity, inclusive.
/// 2. `0 <= self.head < N`: The head index is always within the array bounds.
/// 3. `0 <= self.tail < N`: The tail index is always within the array bounds.
/// 4. `self.head == (self.tail + self.len) % N`: The head is advanced from the
///    tail by the length, modulo N.
/// 5. The `self.len` populated elements are stored in `buffer` at indices
///    `(self.tail) % N`, `(self.tail + 1) % N`, ..., `(self.tail + self.len - 1) % N`.
///
/// The case `N = 0` is valid and handled; `push_back` will always return
/// `Error::ResourceExhausted` and `pop_front` will always return `None`,
/// preventing any out-of-bounds access or division by zero.
///
// TODO: The const generic N could potentially lead to monomorphization
// bloat, but LTO may alleviate the bloat.  Investigate during the
// next round of size optimizations.
pub struct CircularBuffer<T, const N: usize> {
    buffer: [MaybeUninit<T>; N],
    head: usize,
    tail: usize,
    len: usize,
}

impl<T, const N: usize> CircularBuffer<T, N> {
    /// Creates a new, empty circular buffer.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            buffer: [const { MaybeUninit::uninit() }; N],
            head: 0,
            tail: 0,
            len: 0,
        }
    }

    /// Returns `true` if the buffer is empty.
    pub const fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Returns `true` if the buffer is full.
    pub const fn is_full(&self) -> bool {
        self.len == N
    }

    /// Returns the number of elements in the buffer.
    pub const fn len(&self) -> usize {
        self.len
    }

    /// Returns the capacity of the buffer.
    pub const fn capacity(&self) -> usize {
        N
    }

    /// Pushes an element to the back of the buffer.
    ///
    /// If the buffer is full, `Error::ResourceExhausted` is returned.
    pub fn push_back(&mut self, item: T) -> Result<()> {
        if self.is_full() {
            return Err(Error::ResourceExhausted);
        }
        // SAFETY:
        // 1. By Invariant 1 (`0 <= self.len <= N`) and the `is_full()` check,
        //    we know `self.len < N`.
        // 2. By Invariant 2 (`0 <= self.head < N`), `self.head` is a valid
        //    index (this is only reachable if `N > 0`).
        // 3. By Invariants 5, the slot at `self.head` is empty
        //    because `self.len < N`.
        unsafe {
            self.buffer.get_unchecked_mut(self.head).write(item);
        }
        self.head = (self.head + 1) % N;
        self.len += 1;
        Ok(())
    }

    /// Removes an element from the **front** of the buffer (dequeue).
    ///
    /// Returns `None` if the buffer is empty.
    pub fn pop_front(&mut self) -> Option<T> {
        if self.is_empty() {
            return None;
        }
        // SAFETY:
        // 1. By Invariant 1 (`0 <= self.len <= N`) and the `is_empty()` check,
        //    we know `self.len > 0`.
        // 2. By Invariant 3 (`0 <= self.tail < N`), `self.tail` is a valid
        //    index (this is only reachable if `N > 0`).
        // 3. By Invariant 5, the slot at `self.tail` is the first of
        //    `self.len` initialized elements.
        let item = unsafe { self.buffer.get_unchecked(self.tail).assume_init_read() };
        self.tail = (self.tail + 1) % N;
        self.len -= 1;
        Some(item)
    }
}

impl<T, const N: usize> Default for CircularBuffer<T, N> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T, const N: usize> Drop for CircularBuffer<T, N> {
    fn drop(&mut self) {
        while self.pop_front().is_some() {}
    }
}

#[cfg(test)]
mod tests {
    use pw_status::Error;
    use unittest::test;

    use super::*;

    #[test]
    fn new_buffer_is_empty() -> unittest::Result<()> {
        let buffer = CircularBuffer::<u32, 8>::new();
        unittest::assert_true!(buffer.is_empty());
        unittest::assert_false!(buffer.is_full());
        unittest::assert_eq!(buffer.len(), 0);
        unittest::assert_eq!(buffer.capacity(), 8);

        Ok(())
    }

    #[test]
    fn push_and_pop_one_element() -> unittest::Result<()> {
        let mut buffer = CircularBuffer::<u32, 8>::new();

        buffer.push_back(42).unwrap();
        unittest::assert_false!(buffer.is_empty());
        unittest::assert_eq!(buffer.len(), 1);

        let item = buffer.pop_front();
        unittest::assert_eq!(item, Some(42));
        unittest::assert_true!(buffer.is_empty());
        unittest::assert_eq!(buffer.len(), 0);

        Ok(())
    }

    #[test]
    fn fill_and_empty_buffer() -> unittest::Result<()> {
        let mut buffer = CircularBuffer::<u32, 4>::new();

        for i in 0..4 {
            buffer.push_back(i).unwrap();
        }
        unittest::assert_true!(buffer.is_full());
        unittest::assert_eq!(buffer.len(), 4);

        for i in 0..4 {
            let item = buffer.pop_front();
            unittest::assert_eq!(item, Some(i));
        }
        unittest::assert_true!(buffer.is_empty());

        Ok(())
    }

    #[test]
    fn push_to_full_buffer_returns_error() -> unittest::Result<()> {
        let mut buffer = CircularBuffer::<u32, 2>::new();

        buffer.push_back(1).unwrap();
        buffer.push_back(2).unwrap();
        unittest::assert_true!(buffer.is_full());

        let result = buffer.push_back(3);
        unittest::assert_true!(result.is_err());
        unittest::assert_eq!(result.unwrap_err(), Error::ResourceExhausted);

        Ok(())
    }

    #[test]
    fn pop_from_empty_buffer_returns_none() -> unittest::Result<()> {
        let mut buffer = CircularBuffer::<u32, 2>::new();

        let item = buffer.pop_front();
        unittest::assert_true!(item.is_none());

        Ok(())
    }

    #[test]
    fn wrap_around_behavior() -> unittest::Result<()> {
        let mut buffer = CircularBuffer::<u32, 3>::new();

        buffer.push_back(1).unwrap();
        buffer.push_back(2).unwrap();
        buffer.push_back(3).unwrap();
        unittest::assert_eq!(buffer.pop_front().unwrap(), 1);

        buffer.push_back(4).unwrap();
        unittest::assert_true!(buffer.is_full());
        unittest::assert_eq!(buffer.pop_front().unwrap(), 2);
        unittest::assert_eq!(buffer.pop_front().unwrap(), 3);
        unittest::assert_eq!(buffer.pop_front().unwrap(), 4);
        unittest::assert_true!(buffer.is_empty());

        Ok(())
    }

    #[test]
    fn drop_drops_elements() -> unittest::Result<()> {
        use core::cell::Cell;

        struct DropCounter<'a> {
            count: &'a Cell<u32>,
        }

        impl<'a> Drop for DropCounter<'a> {
            fn drop(&mut self) {
                self.count.set(self.count.get() + 1);
            }
        }

        let counter = Cell::new(0);
        {
            let mut buffer = CircularBuffer::<DropCounter, 4>::new();
            buffer.push_back(DropCounter { count: &counter }).unwrap();
            buffer.push_back(DropCounter { count: &counter }).unwrap();
        }
        unittest::assert_eq!(counter.get(), 2);
        Ok(())
    }
}
