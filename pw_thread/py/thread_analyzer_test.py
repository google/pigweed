#!/usr/bin/env python3
# Copyright 2021 The Pigweed Authors
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
"""Tests for the thread analyzer."""

import unittest
from pw_thread.thread_analyzer import StackInfo, ThreadSnapshotAnalyzer
from pw_thread_protos import thread_pb2


class StackInfoTest(unittest.TestCase):
    """Tests that the StackInfo class produces expected results."""
    def test_empty_thread(self):
        stack_info = StackInfo(thread_pb2.Thread())
        expected = '\n'.join(
            ('Stack info',
             '  Stack cur used:  0x???????? - 0x???????? (size unknown)',
             '  Stack max used:  size unknown',
             '  Stack limits:    0x???????? - 0x???????? (size unknown)'))
        self.assertFalse(stack_info.has_stack_size_limit())
        self.assertFalse(stack_info.has_stack_used())
        self.assertEqual(expected, str(stack_info))

    def test_thread_with_stack_pointer(self):
        thread = thread_pb2.Thread()
        thread.stack_pointer = 0x5AC6A86C
        stack_info = StackInfo(thread)

        expected = '\n'.join(
            ('Stack info',
             '  Stack cur used:  0x???????? - 0x5ac6a86c (size unknown)',
             '  Stack max used:  size unknown',
             '  Stack limits:    0x???????? - 0x???????? (size unknown)'))
        self.assertFalse(stack_info.has_stack_size_limit())
        self.assertFalse(stack_info.has_stack_used())
        self.assertEqual(expected, str(stack_info))

    def test_thread_with_stack_usage(self):
        thread = thread_pb2.Thread()
        thread.stack_start_pointer = 0x5AC6B86C
        thread.stack_pointer = 0x5AC6A86C
        stack_info = StackInfo(thread)

        expected = '\n'.join(
            ('Stack info',
             '  Stack cur used:  0x5ac6b86c - 0x5ac6a86c (4096 bytes)',
             '  Stack max used:  size unknown',
             '  Stack limits:    0x5ac6b86c - 0x???????? (size unknown)'))
        self.assertFalse(stack_info.has_stack_size_limit())
        self.assertTrue(stack_info.has_stack_used())
        self.assertEqual(expected, str(stack_info))

    def test_thread_with_all_stack_info(self):
        thread = thread_pb2.Thread()
        thread.stack_start_pointer = 0x5AC6B86C
        thread.stack_end_pointer = 0x5AC6986C
        thread.stack_pointer = 0x5AC6A86C
        stack_info = StackInfo(thread)

        expected = '\n'.join(
            ('Stack info',
             '  Stack cur used:  0x5ac6b86c - 0x5ac6a86c (4096 bytes, 50.00%)',
             '  Stack max used:  size unknown',
             '  Stack limits:    0x5ac6b86c - 0x5ac6986c (8192 bytes)'))
        self.assertTrue(stack_info.has_stack_size_limit())
        self.assertTrue(stack_info.has_stack_used())
        self.assertEqual(expected, str(stack_info))


class ThreadSnapshotAnalyzerTest(unittest.TestCase):
    """Tests that the ThreadSnapshotAnalyzer class produces expected results."""
    def test_no_threads(self):
        analyzer = ThreadSnapshotAnalyzer(thread_pb2.SnapshotThreadInfo())
        self.assertEqual('', str(analyzer))

    def test_one_empty_thread(self):
        snapshot = thread_pb2.SnapshotThreadInfo()
        snapshot.threads.append(thread_pb2.Thread())
        expected = '\n'.join((
            'Thread State',
            '  1 thread running.',
            '',
            'Thread (UNKNOWN): [unnamed thread]',
            'Stack info',
            '  Stack cur used:  0x???????? - 0x???????? (size unknown)',
            '  Stack max used:  size unknown',
            '  Stack limits:    0x???????? - 0x???????? (size unknown)',
            '',
        ))
        analyzer = ThreadSnapshotAnalyzer(snapshot)
        self.assertEqual(analyzer.active_thread(), None)
        self.assertEqual(str(ThreadSnapshotAnalyzer(snapshot)), expected)

    def test_two_threads(self):
        """Ensures multiple threads are printed correctly."""
        snapshot = thread_pb2.SnapshotThreadInfo()

        temp_thread = thread_pb2.Thread()
        temp_thread.name = 'Idle'.encode()
        temp_thread.state = thread_pb2.ThreadState.Enum.READY
        temp_thread.stack_start_pointer = 0x2001ac00
        temp_thread.stack_end_pointer = 0x2001aa00
        temp_thread.stack_pointer = 0x2001ab0c
        temp_thread.stack_pointer_est_peak = 0x2001aa00
        snapshot.threads.append(temp_thread)

        temp_thread = thread_pb2.Thread()
        temp_thread.name = 'Main/Handler'.encode()
        temp_thread.stack_start_pointer = 0x2001b000
        temp_thread.stack_pointer = 0x2001ae20
        temp_thread.state = thread_pb2.ThreadState.Enum.INTERRUPT_HANDLER
        snapshot.threads.append(temp_thread)

        expected = '\n'.join((
            'Thread State',
            '  2 threads running.',
            '',
            'Thread (READY): Idle',
            'Stack info',
            '  Stack cur used:  0x2001ac00 - 0x2001ab0c (244 bytes, 47.66%)',
            '  Stack max used:  512 bytes, 100.00%',
            '  Stack limits:    0x2001ac00 - 0x2001aa00 (512 bytes)',
            '',
            'Thread (INTERRUPT_HANDLER): Main/Handler',
            'Stack info',
            '  Stack cur used:  0x2001b000 - 0x2001ae20 (480 bytes)',
            '  Stack max used:  size unknown',
            '  Stack limits:    0x2001b000 - 0x???????? (size unknown)',
            '',
        ))
        analyzer = ThreadSnapshotAnalyzer(snapshot)
        self.assertEqual(analyzer.active_thread(), None)
        self.assertEqual(str(ThreadSnapshotAnalyzer(snapshot)), expected)

    def test_active_thread(self):
        """Ensures the 'active' thread is highlighted."""
        snapshot = thread_pb2.SnapshotThreadInfo()

        temp_thread = thread_pb2.Thread()
        temp_thread.name = 'Idle'.encode()
        temp_thread.state = thread_pb2.ThreadState.Enum.READY
        temp_thread.stack_start_pointer = 0x2001ac00
        temp_thread.stack_end_pointer = 0x2001aa00
        temp_thread.stack_pointer = 0x2001ab0c
        temp_thread.stack_pointer_est_peak = 0x2001ac00 + 0x100
        snapshot.threads.append(temp_thread)

        temp_thread = thread_pb2.Thread()
        temp_thread.name = 'Main/Handler'.encode()
        temp_thread.active = True
        temp_thread.stack_start_pointer = 0x2001b000
        temp_thread.stack_pointer = 0x2001ae20
        temp_thread.stack_pointer_est_peak = 0x2001b000 + 0x200
        temp_thread.state = thread_pb2.ThreadState.Enum.INTERRUPT_HANDLER
        snapshot.threads.append(temp_thread)

        expected = '\n'.join((
            'Thread State',
            '  2 threads running, Main/Handler active at the time of capture.',
            '                     ~~~~~~~~~~~~',
            '',
            # Ensure the active thread is moved to the top of the list.
            'Thread (INTERRUPT_HANDLER): Main/Handler <-- [ACTIVE]',
            'Stack info',
            '  Stack cur used:  0x2001b000 - 0x2001ae20 (480 bytes)',
            '  Stack max used:  512 bytes',
            '  Stack limits:    0x2001b000 - 0x???????? (size unknown)',
            '',
            'Thread (READY): Idle',
            'Stack info',
            '  Stack cur used:  0x2001ac00 - 0x2001ab0c (244 bytes, 47.66%)',
            '  Stack max used:  256 bytes, 50.00%',
            '  Stack limits:    0x2001ac00 - 0x2001aa00 (512 bytes)',
            '',
        ))
        analyzer = ThreadSnapshotAnalyzer(snapshot)

        # Ensure the active thread is found.
        self.assertEqual(analyzer.active_thread(), temp_thread)
        self.assertEqual(str(ThreadSnapshotAnalyzer(snapshot)), expected)


if __name__ == '__main__':
    unittest.main()
