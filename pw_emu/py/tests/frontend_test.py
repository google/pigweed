#!/usr/bin/env python
# Copyright 2023 The Pigweed Authors
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
"""Emulator API tests."""

import unittest

from typing import Any, Dict

from pw_emu.core import (
    ConfigError,
    InvalidChannelType,
    InvalidProperty,
    InvalidPropertyPath,
)
from mock_emu_frontend import _mock_emu
from tests.common import ConfigHelperWithEmulator


class TestEmulator(ConfigHelperWithEmulator):
    """Test Emulator APIs."""

    _config = {
        'emulators': {
            'mock-emu': {
                'launcher': 'mock_emu_frontend.MockEmuLauncher',
                'connector': 'mock_emu_frontend.MockEmuConnector',
            }
        },
        'mock-emu': {
            'gdb_channel': True,
        },
        'gdb': _mock_emu + ['--exit', '--'],
        'targets': {'test-target': {'mock-emu': {}}},
    }

    def setUp(self) -> None:
        super().setUp()
        self._emu.start('test-target')

    def tearDown(self) -> None:
        self._emu.stop()
        super().tearDown()

    def test_gdb_target_remote(self) -> None:
        output = self._emu.run_gdb_cmds([]).stdout
        host, port = self._emu.get_channel_addr('gdb')
        self.assertTrue(f'{host}:{port}' in output.decode('utf-8'))

    def test_gdb_commands(self) -> None:
        output = self._emu.run_gdb_cmds(['test_gdb_cmd']).stdout
        self.assertTrue(
            output and '-ex test_gdb_cmd' in output.decode('utf-8'), output
        )

    def test_gdb_executable(self) -> None:
        output = self._emu.run_gdb_cmds([], 'test_gdb_exec').stdout
        self.assertTrue('test_gdb_exec' in output.decode('utf-8'))

    def test_gdb_pause(self) -> None:
        output = self._emu.run_gdb_cmds([], pause=True).stdout
        self.assertTrue('-ex disconnect' in output.decode('utf-8'))

    # Minimal testing for APIs that are straight wrappers over Connector APIs.
    def test_running(self) -> None:
        self.assertTrue(self._emu.running())

    def test_reset(self) -> None:
        self._emu.reset()

    def test_cont(self) -> None:
        self._emu.cont()

    def test_list_properties(self) -> None:
        with self.assertRaises(InvalidPropertyPath):
            self._emu.list_properties('invalid path')
        self.assertEqual(self._emu.list_properties('path1'), ['prop1'])

    def test_get_property(self) -> None:
        with self.assertRaises(InvalidProperty):
            self._emu.get_property('path1', 'invalid property')
        with self.assertRaises(InvalidPropertyPath):
            self._emu.get_property('invalid path', 'prop1')
        self.assertEqual(self._emu.get_property('path1', 'prop1'), 'val1')

    def test_set_property(self) -> None:
        with self.assertRaises(InvalidPropertyPath):
            self._emu.set_property('invalid path', 'prop1', 'test')
        with self.assertRaises(InvalidProperty):
            self._emu.set_property('path1', 'invalid property', 'test')
        self._emu.set_property('path1', 'prop1', 'val2')
        self.assertEqual(self._emu.get_property('path1', 'prop1'), 'val2')

    def test_get_channel_type(self) -> None:
        self.assertEqual(self._emu.get_channel_type('gdb'), 'tcp')

    def test_get_channel_path(self) -> None:
        with self.assertRaises(InvalidChannelType):
            self._emu.get_channel_path('gdb')

    def test_get_channel_addr(self) -> None:
        self.assertEqual(len(self._emu.get_channel_addr('gdb')), 2)

    def test_channel_stream(self) -> None:
        with self._emu.get_channel_stream('gdb') as _:
            pass


class TestGdbEmptyConfig(ConfigHelperWithEmulator):
    """Check that ConfigError is raised when running gdb with an empty
    gdb config.

    """

    _config: Dict[str, Any] = {
        'emulators': {
            'mock-emu': {
                'launcher': 'mock_emu_frontend.MockEmuLauncher',
                'connector': 'mock_emu_frontend.MockEmuConnector',
            }
        },
        'targets': {'test-target': {'mock-emu': {}}},
    }

    def setUp(self) -> None:
        super().setUp()
        self._emu.start('test-target')

    def tearDown(self) -> None:
        self._emu.stop()
        super().tearDown()

    def test_gdb_config_error(self) -> None:
        with self.assertRaises(ConfigError):
            self._emu.run_gdb_cmds([])


if __name__ == '__main__':
    unittest.main()
