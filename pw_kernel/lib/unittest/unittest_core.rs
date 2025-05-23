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

list::define_adapter!(pub TestAdapter => Test.link);

static mut TEST_LIST: ForeignList<Test, TestAdapter> = ForeignList::new();

// All accesses to test list go through this function.  This gives us a
// single point of ownership of TEST_LIST and keeps us from leaking references
// to it.
fn access_test_list<F>(callback: F)
where
    F: FnOnce(&mut ForeignList<Test, TestAdapter>),
{
    // Safety: Tests are single threaded for now.  This assumption needs to be
    // revisited.
    #[allow(static_mut_refs)]
    callback(unsafe { &mut TEST_LIST })
}

pub fn add_test(test: ForeignBox<Test>) {
    access_test_list(|test_list| test_list.push_back(test))
}

fn for_each_test<F>(mut callback: F)
where
    F: FnMut(&Test),
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

#[derive(Eq, PartialEq)]
pub enum TestsResult {
    AllPassed,
    SomeFailed,
}

pub fn run_bare_metal_tests() -> TestsResult {
    run_tests(TestSet::BareMetal)
}

pub fn run_all_tests() -> TestsResult {
    let bare_metal_result = run_tests(TestSet::BareMetal);
    let kernel_result = run_tests(TestSet::Kernel);

    use TestsResult::*;
    match (bare_metal_result, kernel_result) {
        (AllPassed, AllPassed) => AllPassed,
        _ => SomeFailed,
    }
}

fn run_tests(set: TestSet) -> TestsResult {
    let mut result = TestsResult::AllPassed;
    for_each_test(|test| {
        if test.set != set {
            return;
        }

        pw_log::info!("🔄 [{}] RUNNING", test.name as &str);
        if let Err(e) = (test.test_fn)() {
            pw_log::error!("❌ [{}] FAILED", test.name as &str);
            pw_log::error!("❌ ├─ {}:{}:", e.file as &str, e.line as u32);
            pw_log::error!("❌ └─ {}", e.message as &str);
            result = TestsResult::SomeFailed;
        } else {
            pw_log::info!("✅ [{}] PASSED", test.name as &str);
        }
    });
    result
}

pub struct TestError {
    pub file: &'static str,
    pub line: u32,
    pub message: &'static str,
}

pub type Result<T> = core::result::Result<T, TestError>;

pub type TestFn = fn() -> Result<()>;

/// Which set of tests is this test a member of?
#[derive(PartialEq)]
pub enum TestSet {
    /// Bare metal tests run with or without a kernel.
    BareMetal,
    /// Kernel tests only run when a kernel is present and has been initialized.
    Kernel,
}

pub struct Test {
    name: &'static str,
    test_fn: TestFn,
    set: TestSet,
    link: Link,
}

impl Test {
    pub const fn new(name: &'static str, test_fn: TestFn, set: TestSet) -> Self {
        Self {
            name,
            test_fn,
            set,
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
unsafe impl Send for Test {}
unsafe impl Sync for Test {}

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
