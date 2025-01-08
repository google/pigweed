#!/usr/bin/env python3
# Copyright 2022 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Tests for the timestamp analyzer."""

import unittest
from pw_chrono.timestamp_analyzer import process_snapshot
from pw_chrono_protos import chrono_pb2
from pw_tokenizer import detokenize, tokens


class TimestampTest(unittest.TestCase):
    """Test for the timestamp analyzer."""

    def test_no_timepoint(self):
        snapshot = chrono_pb2.SnapshotTimestamps()
        self.assertEqual(
            '', str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_unknown_epoch_type(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=0,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1,
                epoch_type=chrono_pb2.EpochType.Enum.UNKNOWN,
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  Timestamp (epoch UNKNOWN): 0 s',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_with_time_since_boot(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=8640015,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000,
                epoch_type=chrono_pb2.EpochType.Enum.TIME_SINCE_BOOT,
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  Time since boot: 2:24:00.015000 (8640015 ms)',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_with_utc_wall_clock(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=8640015,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000,
                epoch_type=chrono_pb2.EpochType.Enum.UTC_WALL_CLOCK,
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  UTC time: 1970-01-01 02:24:00.015000+00:00 (8640015 ms)',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_with_name(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=51140716,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000000,
                epoch_type=chrono_pb2.EpochType.Enum.UNKNOWN,
                name='High resolution clock'.encode('utf-8'),
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  High resolution clock (epoch UNKNOWN): 51140716 us',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_with_tokenized_name(self):
        token_db = tokens.Database(
            [
                tokens.TokenizedStringEntry(1, 'Peer device clock'),
            ]
        )
        detokenizer = detokenize.Detokenizer(token_db)

        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=51140716,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000000,
                epoch_type=chrono_pb2.EpochType.Enum.UNKNOWN,
                name=b'\x01\x00\x00\x00',
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  Peer device clock (epoch UNKNOWN): 51140716 us',
                '',
            )
        )

        self.assertEqual(
            expected,
            str(process_snapshot(snapshot.SerializeToString(), detokenizer)),
        )

    def test_timestamp_with_tai_clock(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=1733882018,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000,
                epoch_type=chrono_pb2.EpochType.Enum.TAI_WALL_CLOCK,
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  Timestamp (epoch TAI_WALL_CLOCK): 1733882018 ms',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_with_unrecognized_epoch(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=3424,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1,
                epoch_type=chrono_pb2.EpochType.Enum.ValueType(15),
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamp',
                '  Timestamp (epoch 15): 3424 s',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_invalid_timestamp(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=1,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=0,
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = ''

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_no_timestamp(self):
        snapshot = chrono_pb2.SnapshotTimestamps()

        expected = ''

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )

    def test_timestamp_with_time_since_boot_and_utc_wall_clock(self):
        """Test multiple timestamps."""
        snapshot = chrono_pb2.SnapshotTimestamps()

        time_point = chrono_pb2.TimePoint(
            timestamp=8640000,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000,
                epoch_type=chrono_pb2.EpochType.Enum.TIME_SINCE_BOOT,
            ),
        )
        snapshot.timestamps.append(time_point)

        time_point = chrono_pb2.TimePoint(
            timestamp=8640000,
            clock_parameters=chrono_pb2.ClockParameters(
                tick_period_seconds_numerator=1,
                tick_period_seconds_denominator=1000,
                epoch_type=chrono_pb2.EpochType.Enum.UTC_WALL_CLOCK,
            ),
        )
        snapshot.timestamps.append(time_point)

        expected = '\n'.join(
            (
                'Snapshot capture timestamps',
                '  Time since boot: 2:24:00 (8640000 ms)',
                '  UTC time: 1970-01-01 02:24:00+00:00 (8640000 ms)',
                '',
            )
        )

        self.assertEqual(
            expected, str(process_snapshot(snapshot.SerializeToString()))
        )


if __name__ == '__main__':
    unittest.main()
