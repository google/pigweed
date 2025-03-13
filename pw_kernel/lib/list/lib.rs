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
#![cfg_attr(test, no_main)]

use core::ptr::NonNull;

use foreign_box::ForeignBox;

pub mod unsafe_list;

pub use unsafe_list::{Adapter, Link, UnsafeList};

pub struct ForeignList<T, A: Adapter> {
    list: UnsafeList<T, A>,
}

impl<T, A: Adapter> Default for ForeignList<T, A> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T, A: Adapter> ForeignList<T, A> {
    pub const fn new() -> Self {
        Self {
            list: UnsafeList::new(),
        }
    }

    pub fn is_empty(&self) -> bool {
        unsafe { self.list.is_empty() }
    }

    pub fn push_front(&mut self, element: ForeignBox<T>) {
        let element = element.consume();
        unsafe { self.list.push_front_unchecked(element.as_ptr()) }
    }

    pub fn push_back(&mut self, element: ForeignBox<T>) {
        let element = element.consume();
        unsafe { self.list.push_back_unchecked(element.as_ptr()) }
    }

    pub fn pop_head(&mut self) -> Option<ForeignBox<T>> {
        unsafe {
            self.list
                .pop_head()
                .map(|element| ForeignBox::new(NonNull::new_unchecked(element)))
        }
    }

    pub fn for_each<E, F: FnMut(&T) -> Result<(), E>>(&self, callback: F) -> Result<(), E> {
        unsafe { self.list.for_each(callback) }
    }

    /// # Safety
    /// Call ensures the element is a valid point to an instance of T.
    pub unsafe fn remove_element(&mut self, element: NonNull<T>) -> Option<ForeignBox<T>> {
        unsafe {
            self.list
                .unlink_element(element)
                .map(|element| ForeignBox::new(element))
        }
    }
}

impl<T: Ord, A: Adapter> ForeignList<T, A> {
    pub fn sorted_insert(&mut self, element: ForeignBox<T>) {
        let element = element.consume();
        unsafe { self.list.sorted_insert_unchecked(element.as_ptr()) }
    }
}
