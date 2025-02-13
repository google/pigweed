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
#![no_main]
use core::mem::offset_of;

// TODO: add unit tests for push_back and pop_head

use list::*;
use unittest::test;

// `#[repr(C)]` is used to ensure that `link` is at a non-zero offset.
// Previously, without this, the compiler was putting link at the beginning of
// the struct causing `LINK_OFFSET` in the adapter to be zero which obfuscated
// some pointer math bugs.
#[repr(C)]
struct TestMember {
    value: u32,
    link: Link,
}

struct TestAdapter {}
impl Adapter for TestAdapter {
    const LINK_OFFSET: usize = offset_of!(TestMember, link);
}

unsafe fn validate_list(
    list: &UnsafeList<TestMember, TestAdapter>,
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

#[test]
fn new_link_is_not_linked() -> unittest::Result<()> {
    let link = Link::new();
    unittest::assert_false!(link.is_linked());
    unittest::assert_true!(link.is_unlinked());
    Ok(())
}

#[test]
fn new_list_is_empty() -> unittest::Result<()> {
    let list = UnsafeList::<TestMember, TestAdapter>::new();
    unittest::assert_true!(unsafe { list.is_empty() });
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unittest::assert_false!(unsafe { list.is_empty() });

    unsafe { validate_list(&list, &[1, 2]) }
}

#[test]
fn unlink_removes_head_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.unlink_element(&element1) };

    unsafe { validate_list(&list, &[2, 3]) }
}

#[test]
fn unlink_removes_tail_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.unlink_element(&element3) };

    unsafe { validate_list(&list, &[1, 2]) }
}

#[test]
fn unlink_removes_middle_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.unlink_element(&element2) };

    unsafe { validate_list(&list, &[1, 3]) }
}

#[test]
fn filter_removes_nothing_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.filter(|_| true) };

    unsafe { validate_list(&list, &[1, 2, 3]) }
}

#[test]
fn filter_removes_everything_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.filter(|_| false) };

    unsafe { validate_list(&list, &[]) }
}

#[test]
fn filter_removes_head_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.filter(|element| element.value != 1) };

    unsafe { validate_list(&list, &[2, 3]) }
}

#[test]
fn filter_removes_middle_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.filter(|element| element.value != 2) };

    unsafe { validate_list(&list, &[1, 3]) }
}

#[test]
fn filter_removes_tail_correctly() -> unittest::Result<()> {
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

    let mut list = UnsafeList::<TestMember, TestAdapter>::new();
    unsafe { list.push_front_unchecked(&mut element3) };
    unsafe { list.push_front_unchecked(&mut element2) };
    unsafe { list.push_front_unchecked(&mut element1) };

    unsafe { list.filter(|element| element.value != 3) };

    unsafe { validate_list(&list, &[1, 2]) }
}
