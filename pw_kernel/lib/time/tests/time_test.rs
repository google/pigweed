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

use unittest::test;

use time::*;

struct TestClock;

impl Clock for TestClock {
    const TICKS_PER_SEC: u64 = 1_000;

    fn now() -> Instant<Self> {
        Instant::from_ticks(0)
    }
}

struct HighResTestClock;
impl Clock for HighResTestClock {
    const TICKS_PER_SEC: u64 = 1_000_000_000;

    fn now() -> Instant<Self> {
        Instant::from_ticks(0)
    }
}

#[test]
fn duration_constructors_return_correct_values() -> unittest::Result<()> {
    unittest::assert_eq!(Duration::<TestClock>::from_secs(1234).ticks(), 1_234_000);
    unittest::assert_eq!(Duration::<TestClock>::from_millis(1234).ticks(), 1_234);
    unittest::assert_eq!(Duration::<TestClock>::from_micros(1234).ticks(), 1);
    unittest::assert_eq!(Duration::<TestClock>::from_nanos(1234).ticks(), 0);

    unittest::assert_eq!(Duration::<HighResTestClock>::from_nanos(1234).ticks(), 1234);
    Ok(())
}

#[test]
fn duration_checked_addition_returns_correct_values() -> unittest::Result<()> {
    let ten_ms = Duration::<TestClock>::from_millis(10);
    let one_ms = Duration::<TestClock>::from_millis(1);

    unittest::assert_eq!(
        ten_ms.checked_add(one_ms),
        Some(Duration::<TestClock>::from_millis(11))
    );

    unittest::assert_eq!(
        one_ms.checked_add(ten_ms),
        Some(Duration::<TestClock>::from_millis(11))
    );

    unittest::assert_eq!(Duration::<TestClock>::MAX.checked_add(one_ms), None);

    unittest::assert_eq!(
        Duration::<TestClock>::MIN.checked_add(Duration::from_millis(-1)),
        None
    );
    Ok(())
}

#[test]
fn duration_checked_subtraction_returns_correct_values() -> unittest::Result<()> {
    let ten_ms = Duration::<TestClock>::from_millis(10);
    let one_ms = Duration::<TestClock>::from_millis(1);

    unittest::assert_eq!(
        ten_ms.checked_sub(one_ms),
        Some(Duration::<TestClock>::from_millis(9))
    );

    unittest::assert_eq!(
        one_ms.checked_sub(ten_ms),
        Some(Duration::<TestClock>::from_millis(-9))
    );

    unittest::assert_eq!(
        Duration::<TestClock>::MAX.checked_sub(Duration::from_millis(-1)),
        None
    );

    unittest::assert_eq!(
        Duration::<TestClock>::MIN.checked_sub(Duration::from_millis(1)),
        None
    );
    Ok(())
}

#[test]
fn instant_subtraction_returns_correct_values() -> unittest::Result<()> {
    let ten_ms = Instant::from_ticks(10 * <TestClock as Clock>::TICKS_PER_SEC / 1000);
    let one_ms = Instant::from_ticks(<TestClock as Clock>::TICKS_PER_SEC / 1000);

    unittest::assert_eq!(ten_ms - one_ms, Duration::<TestClock>::from_millis(9));
    unittest::assert_eq!(one_ms - ten_ms, Duration::<TestClock>::from_millis(-9));

    Ok(())
}

#[test]
fn instant_checked_duration_addition_returns_correct_values() -> unittest::Result<()> {
    let instant_eleven_ms =
        Instant::<TestClock>::from_ticks(11 * <TestClock as Clock>::TICKS_PER_SEC / 1000);
    let instant_ten_ms =
        Instant::<TestClock>::from_ticks(10 * <TestClock as Clock>::TICKS_PER_SEC / 1000);
    let instant_nine_ms =
        Instant::<TestClock>::from_ticks(9 * <TestClock as Clock>::TICKS_PER_SEC / 1000);

    let duration_one_ms = Duration::<TestClock>::from_millis(1);
    let duration_minus_one_ms = Duration::<TestClock>::from_millis(-1);

    unittest::assert_eq!(
        instant_ten_ms.checked_add_duration(duration_one_ms),
        Some(instant_eleven_ms)
    );

    unittest::assert_eq!(
        instant_ten_ms.checked_add_duration(duration_minus_one_ms),
        Some(instant_nine_ms)
    );

    unittest::assert_eq!(
        Instant::<TestClock>::MAX.checked_add_duration(duration_one_ms),
        None
    );

    unittest::assert_eq!(
        Instant::<TestClock>::MIN.checked_add_duration(duration_minus_one_ms),
        None
    );

    Ok(())
}

#[test]
fn instant_checked_duration_subtraction_returns_correct_values() -> unittest::Result<()> {
    let instant_eleven_ms =
        Instant::<TestClock>::from_ticks(11 * <TestClock as Clock>::TICKS_PER_SEC / 1000);
    let instant_ten_ms =
        Instant::<TestClock>::from_ticks(10 * <TestClock as Clock>::TICKS_PER_SEC / 1000);
    let instant_nine_ms =
        Instant::<TestClock>::from_ticks(9 * <TestClock as Clock>::TICKS_PER_SEC / 1000);

    let duration_one_ms = Duration::<TestClock>::from_millis(1);
    let duration_minus_one_ms = Duration::<TestClock>::from_millis(-1);

    unittest::assert_eq!(
        instant_ten_ms.checked_sub_duration(duration_one_ms),
        Some(instant_nine_ms)
    );

    unittest::assert_eq!(
        instant_ten_ms.checked_sub_duration(duration_minus_one_ms),
        Some(instant_eleven_ms)
    );

    unittest::assert_eq!(
        Instant::<TestClock>::MAX.checked_sub_duration(duration_minus_one_ms),
        None
    );

    unittest::assert_eq!(
        Instant::<TestClock>::MIN.checked_sub_duration(duration_one_ms),
        None
    );

    Ok(())
}
