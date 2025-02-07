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

use pw_status::*;
use unittest::test;

#[test]
fn test_status_code() -> core::result::Result<(), unittest::TestError> {
    unittest::assert_eq!(Result::Ok(()).status_code(), 0);
    unittest::assert_eq!(Result::<()>::Err(Error::Cancelled).status_code(), 1);
    unittest::assert_eq!(Result::<()>::Err(Error::Unknown).status_code(), 2);
    unittest::assert_eq!(Result::<()>::Err(Error::InvalidArgument).status_code(), 3);
    unittest::assert_eq!(Result::<()>::Err(Error::DeadlineExceeded).status_code(), 4);
    unittest::assert_eq!(Result::<()>::Err(Error::NotFound).status_code(), 5);
    unittest::assert_eq!(Result::<()>::Err(Error::AlreadyExists).status_code(), 6);
    unittest::assert_eq!(Result::<()>::Err(Error::PermissionDenied).status_code(), 7);
    unittest::assert_eq!(Result::<()>::Err(Error::ResourceExhausted).status_code(), 8);
    unittest::assert_eq!(
        Result::<()>::Err(Error::FailedPrecondition).status_code(),
        9
    );
    unittest::assert_eq!(Result::<()>::Err(Error::Aborted).status_code(), 10);
    unittest::assert_eq!(Result::<()>::Err(Error::OutOfRange).status_code(), 11);
    unittest::assert_eq!(Result::<()>::Err(Error::Unimplemented).status_code(), 12);
    unittest::assert_eq!(Result::<()>::Err(Error::Internal).status_code(), 13);
    unittest::assert_eq!(Result::<()>::Err(Error::Unavailable).status_code(), 14);
    unittest::assert_eq!(Result::<()>::Err(Error::DataLoss).status_code(), 15);
    unittest::assert_eq!(Result::<()>::Err(Error::Unauthenticated).status_code(), 16);
    Ok(())
}
