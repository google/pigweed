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
    #[must_use]
    pub const fn new() -> Self {
        Self {
            list: UnsafeList::new(),
        }
    }

    #[must_use]
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

#[cfg(test)]
mod tests {
    use core::ptr::NonNull;

    use foreign_box::ForeignBox;
    use unittest::test;

    use super::*;

    // `#[repr(C)]` is used to ensure that `link` is at a non-zero offset.
    // Previously, without this, the compiler was putting link at the beginning of
    // the struct causing `LINK_OFFSET` in the adapter to be zero which obfuscated
    // some pointer math bugs.
    #[repr(C)]
    struct TestMember {
        value: u32,
        link: Link,
    }

    impl PartialEq for TestMember {
        fn eq(&self, other: &Self) -> bool {
            self.value == other.value
        }
    }

    impl Eq for TestMember {}

    impl PartialOrd for TestMember {
        fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
            Some(self.value.cmp(&other.value))
        }
    }

    impl Ord for TestMember {
        fn cmp(&self, other: &Self) -> core::cmp::Ordering {
            self.value.cmp(&other.value)
        }
    }

    define_adapter!(TestAdapter => TestMember.link);

    fn validate_list(
        list: &ForeignList<TestMember, TestAdapter>,
        expected_values: &[u32],
    ) -> unittest::Result<()> {
        let mut index = 0;
        list.for_each(|element| {
            unittest::assert_eq!(element.value, expected_values[index]);
            index += 1;
            Ok(())
        })?;

        unittest::assert_eq!(index, expected_values.len());
        Ok(())
    }

    fn drain_list(list: &mut ForeignList<TestMember, TestAdapter>) {
        while let Some(element) = list.pop_head() {
            element.consume();
        }
    }

    #[test]
    fn new_list_is_empty() -> unittest::Result<()> {
        let list = ForeignList::<TestMember, TestAdapter>::new();
        unittest::assert_true!(list.is_empty());
        Ok(())
    }

    #[test]
    fn push_front_adds_in_correct_order() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });

        unittest::assert_false!(list.is_empty());

        validate_list(&list, &[1, 2])?;

        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn push_back_adds_in_correct_order() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.push_back(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.push_back(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });

        unittest::assert_false!(list.is_empty());

        validate_list(&list, &[2, 1])?;

        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn pop_head_removes_correctly() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });

        let e = list.pop_head();
        unittest::assert_true!(e.is_some());
        let e = e.unwrap();
        unittest::assert_eq!(e.value, 3);
        unittest::assert_true!(e.link.is_unlinked());
        e.consume();

        let e = list.pop_head();
        unittest::assert_true!(e.is_some());
        let e = e.unwrap();
        unittest::assert_eq!(e.value, 2);
        unittest::assert_true!(e.link.is_unlinked());
        e.consume();

        let e = list.pop_head();
        unittest::assert_true!(e.is_some());
        let e = e.unwrap();
        unittest::assert_eq!(e.value, 1);
        unittest::assert_true!(e.link.is_unlinked());
        e.consume();

        validate_list(&list, &[])
    }

    #[test]
    fn remove_element_can_remove_head() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });

        let removed_element =
            unsafe { list.remove_element(NonNull::new(&raw mut element1).unwrap()) };
        unittest::assert_true!(removed_element.is_some());
        unittest::assert_eq!(
            removed_element.unwrap().consume(),
            NonNull::new(&raw mut element1).unwrap()
        );

        validate_list(&list, &[2, 3])?;
        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn remove_element_can_remove_middle() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });

        let removed_element =
            unsafe { list.remove_element(NonNull::new(&raw mut element2).unwrap()) };
        unittest::assert_true!(removed_element.is_some());
        unittest::assert_eq!(
            removed_element.unwrap().consume(),
            NonNull::new(&raw mut element2).unwrap()
        );

        validate_list(&list, &[1, 3])?;
        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn remove_element_can_remove_tail() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.push_front(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });

        let removed_element =
            unsafe { list.remove_element(NonNull::new(&raw mut element3).unwrap()) };
        unittest::assert_true!(removed_element.is_some());
        unittest::assert_eq!(
            removed_element.unwrap().consume(),
            NonNull::new(&raw mut element3).unwrap()
        );

        validate_list(&list, &[1, 2])?;
        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn sorted_insert_inserts_sorted_items_in_correct_order() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });
        validate_list(&list, &[1, 2, 3])?;
        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn sorted_insert_inserts_reverse_sorted_items_in_correct_order() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });
        validate_list(&list, &[1, 2, 3])?;
        drain_list(&mut list);
        Ok(())
    }

    #[test]
    fn sorted_insert_inserts_unsorted_items_in_correct_order() -> unittest::Result<()> {
        let mut element1 = TestMember {
            value: 1,
            link: Link::new(),
        };
        let mut element2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element2_2 = TestMember {
            value: 2,
            link: Link::new(),
        };
        let mut element3 = TestMember {
            value: 3,
            link: Link::new(),
        };

        let mut list = ForeignList::<TestMember, TestAdapter>::new();
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element2) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element1) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element3) });
        list.sorted_insert(unsafe { ForeignBox::new_from_ptr(&raw mut element2_2) });
        validate_list(&list, &[1, 2, 2, 3])?;
        drain_list(&mut list);
        Ok(())
    }
}
