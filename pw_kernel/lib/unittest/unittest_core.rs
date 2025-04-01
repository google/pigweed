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

use foreign_box::ForeignBox;
use list::{ForeignList, Link};

pub use foreign_box;
pub use pw_bytes;

list::define_adapter!(pub TestDescAndFnAdapter => TestDescAndFn.link);

static mut TEST_LIST: ForeignList<TestDescAndFn, TestDescAndFnAdapter> = ForeignList::new();

// All accesses to test list go through this function.  This gives us a
// single point of ownership of TEST_LIST and keeps us from leaking references
// to it.
fn access_test_list<F>(callback: F)
where
    F: FnOnce(&mut ForeignList<TestDescAndFn, TestDescAndFnAdapter>),
{
    // Safety: Tests are single threaded for now.  This assumption needs to be
    // revisited.
    #[allow(static_mut_refs)]
    callback(unsafe { &mut TEST_LIST })
}

pub fn add_test(test: ForeignBox<TestDescAndFn>) {
    access_test_list(|test_list| test_list.push_back(test))
}

pub fn for_each_test<F>(mut callback: F)
where
    F: FnMut(&TestDescAndFn),
{
    access_test_list(|test_list| {
        test_list
            .for_each(|t| {
                callback(t);
                Ok::<(), ()>(())
            })
            .unwrap();
    });
}

pub struct TestError {
    pub file: &'static str,
    pub line: u32,
    pub message: &'static str,
}

pub type Result<T> = core::result::Result<T, TestError>;

pub enum TestFn {
    StaticTestFn(fn() -> Result<()>),
}

pub struct TestDesc {
    pub name: &'static str,
}

pub struct TestDescAndFn {
    pub desc: TestDesc,
    pub test_fn: TestFn,
    pub link: Link,
}

impl TestDescAndFn {
    pub const fn new(desc: TestDesc, test_fn: TestFn) -> Self {
        Self {
            desc,
            test_fn,
            link: Link::new(),
        }
    }
}

// We're marking these as send and sync so that we can declare statics with.
// them.  They're not actually Send and Sync because they contain linked list
// pointers but in practice tests are single threaded and these are never sent
// between threads.
//
// A better pattern here must be worked out with intrusive lists of static data
// (for statically declared threads for instance) so we'll revisit this later.
unsafe impl Send for TestDescAndFn {}
unsafe impl Sync for TestDescAndFn {}

#[macro_export]
macro_rules! assert_eq {
    ($a:expr, $b:expr) => {
        if $a != $b {
            return Err(unittest::TestError {
                file: file!(),
                line: line!(),
                message: unittest::pw_bytes::concat_static_strs!(
                    "assert_eq!(",
                    stringify!($a),
                    ", ",
                    stringify!($b),
                    ") failed"
                ),
            });
        }
    };
}

#[macro_export]
macro_rules! assert_ne {
    ($a:expr, $b:expr) => {
        if $a == $b {
            return Err(unittest::TestError {
                file: file!(),
                line: line!(),
                message: unittest::pw_bytes::concat_static_strs!(
                    "assert_ne!(",
                    stringify!($a),
                    ", ",
                    stringify!($b),
                    ") failed"
                ),
            });
        }
    };
}

#[macro_export]
macro_rules! assert_true {
    ($a:expr) => {
        if !$a {
            return Err(unittest::TestError {
                file: file!(),
                line: line!(),
                message: unittest::pw_bytes::concat_static_strs!(
                    "assert_true!(",
                    stringify!($a),
                    ") failed"
                ),
            });
        }
    };
}

#[macro_export]
macro_rules! assert_false {
    ($a:expr) => {
        if $a {
            return Err(unittest::TestError {
                file: file!(),
                line: line!(),
                message: unittest::pw_bytes::concat_static_strs!(
                    "assert_false!(",
                    stringify!($a),
                    ") failed"
                ),
            });
        }
    };
}
