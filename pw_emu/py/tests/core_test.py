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
"""Tests for core / infrastructure code."""

import copy
import io
import json
import os
import socket
import sys
import tempfile
import time
import unittest
from pathlib import Path
from typing import Any, Dict

from unittest.mock import patch

from pw_emu.core import (
    AlreadyRunning,
    Config,
    ConfigError,
    Handles,
    InvalidTarget,
    InvalidChannelName,
    InvalidChannelType,
    Launcher,
)
from mock_emu_frontend import _mock_emu
from tests.common import ConfigHelper


class ConfigHelperWithLauncher(ConfigHelper):
    def setUp(self) -> None:
        super().setUp()
        self._launcher = Launcher.get('mock-emu', Path(self._config_file))


class TestInvalidTarget(ConfigHelperWithLauncher):
    """Check that InvalidTarget is raised with an empty config."""

    _config: Dict[str, Any] = {
        'emulators': {
            'mock-emu': {
                'launcher': 'mock_emu_frontend.MockEmuLauncher',
                'connector': 'mock_emu_frontend.MockEmuConnector',
            }
        },
    }

    def test_invalid_target(self) -> None:
        with self.assertRaises(InvalidTarget):
            self._launcher.start(Path(self._wdir.name), 'test-target')


class TestStart(ConfigHelperWithLauncher):
    """Start tests for valid config."""

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
        self._connector = self._launcher.start(
            Path(self._wdir.name), 'test-target'
        )

    def tearDown(self) -> None:
        self._connector.stop()
        super().tearDown()

    def test_running(self) -> None:
        self.assertTrue(self._connector.running())

    def test_pid_file(self) -> None:
        self.assertTrue(
            os.path.exists(os.path.join(self._wdir.name, 'mock-emu.pid'))
        )

    def test_handles_file(self) -> None:
        self.assertTrue(
            os.path.exists(os.path.join(self._wdir.name, 'handles.json'))
        )

    def test_already_running(self) -> None:
        with self.assertRaises(AlreadyRunning):
            self._launcher.start(Path(self._wdir.name), 'test-target')

    def test_log(self) -> None:
        exp = 'starting mock emulator'
        path = os.path.join(self._wdir.name, 'mock-emu.log')
        deadline = time.monotonic() + 100
        while os.path.getsize(path) < len(exp):
            time.sleep(0.1)
            if time.monotonic() > deadline:
                break

        with open(os.path.join(self._wdir.name, 'mock-emu.log'), 'rt') as file:
            data = file.read()
            self.assertTrue(exp in data, data)


class TestPrePostStartCmds(ConfigHelperWithLauncher):
    """Tests for configurations with pre-start commands."""

    _config: Dict[str, Any] = {
        'emulators': {
            'mock-emu': {
                'launcher': 'mock_emu_frontend.MockEmuLauncher',
                'connector': 'mock_emu_frontend.MockEmuConnector',
            }
        },
        'targets': {
            'test-target': {
                'pre-start-cmds': {
                    'pre-1': _mock_emu + ['pre-1'],
                    'pre-2': _mock_emu + ['$pw_emu_wdir{pre-2}'],
                },
                'post-start-cmds': {
                    'post-1': _mock_emu
                    + ['$pw_emu_channel_path{test_subst_pty}'],
                    'post-2': _mock_emu
                    + [
                        '$pw_emu_channel_host{test_subst_tcp}',
                        '$pw_emu_channel_port{test_subst_tcp}',
                    ],
                },
                'mock-emu': {},
            }
        },
    }

    def setUp(self) -> None:
        super().setUp()
        self._connector = self._launcher.start(
            Path(self._wdir.name), 'test-target'
        )

    def tearDown(self) -> None:
        self._connector.stop()
        super().tearDown()

    def test_running(self) -> None:
        for proc in self._connector.get_procs().keys():
            self.assertTrue(self._connector.proc_running(proc))

    def test_stop(self) -> None:
        self._connector.stop()
        for proc in self._connector.get_procs().keys():
            self.assertFalse(self._connector.proc_running(proc))

    def test_pid_files(self) -> None:
        for proc in ['pre-1', 'pre-2', 'post-1', 'post-2']:
            self.assertTrue(
                os.path.exists(os.path.join(self._wdir.name, f'{proc}.pid'))
            )

    def test_logs(self):
        expect = {
            'pre-1.log': 'pre-1',
            'pre-2.log': os.path.join(self._wdir.name, 'pre-2'),
            'post-1.log': 'pty-path',
            'post-2.log': 'localhost 1234',
        }

        for log, pattern in expect.items():
            path = os.path.join(self._wdir.name, log)
            deadline = time.monotonic() + 100
            while os.path.getsize(path) < len(pattern):
                time.sleep(0.1)
                if time.monotonic() > deadline:
                    break
            with open(os.path.join(self._wdir.name, log)) as file:
                data = file.read()
            self.assertTrue(pattern in data, f'`{pattern}` not in `{data}`')


class TestStop(ConfigHelperWithLauncher):
    """Stop tests for valid config."""

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
        self._connector = self._launcher.start(
            Path(self._wdir.name), 'test-target'
        )
        self._connector.stop()

    def test_pid_files(self) -> None:
        self.assertFalse(
            os.path.exists(os.path.join(self._wdir.name, 'emu.pid'))
        )

    def test_target_file(self) -> None:
        self.assertFalse(
            os.path.exists(os.path.join(self._wdir.name, 'target'))
        )

    def test_running(self) -> None:
        self.assertFalse(self._connector.running())


class TestChannels(ConfigHelperWithLauncher):
    """Test Connector channels APIs."""

    _config: Dict[str, Any] = {
        'emulators': {
            'mock-emu': {
                'launcher': 'mock_emu_frontend.MockEmuLauncher',
                'connector': 'mock_emu_frontend.MockEmuConnector',
            }
        },
        'mock-emu': {
            'tcp_channel': True,
            'pty_channel': sys.platform != 'win32',
        },
        'targets': {'test-target': {'mock-emu': {}}},
    }

    def setUp(self) -> None:
        super().setUp()
        self._connector = self._launcher.start(
            Path(self._wdir.name), 'test-target'
        )

    def tearDown(self) -> None:
        self._connector.stop()
        super().tearDown()

    def test_tcp_channel_addr(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(self._connector.get_channel_addr('tcp'))
        sock.close()

    def test_get_channel_type(self) -> None:
        self.assertEqual(self._connector.get_channel_type('tcp'), 'tcp')
        if sys.platform != 'win32':
            self.assertEqual(self._connector.get_channel_type('pty'), 'pty')
        with self.assertRaises(InvalidChannelName):
            self._connector.get_channel_type('invalid channel')

    def test_pty_channel_path(self) -> None:
        if sys.platform == 'win32':
            self.skipTest('pty not supported on win32')
        self.assertTrue(os.path.exists(self._connector.get_channel_path('pty')))
        with self.assertRaises(InvalidChannelType):
            self._connector.get_channel_path('tcp')

    def _test_stream(self, stream: io.RawIOBase) -> None:
        for char in [b'1', b'2', b'3', b'4']:
            stream.write(char)
            self.assertEqual(stream.read(1), char)

    def test_tcp_stream(self) -> None:
        with self._connector.get_channel_stream('tcp') as stream:
            self._test_stream(stream)

    def test_pty_stream(self) -> None:
        if sys.platform == 'win32':
            self.skipTest('pty not supported on win32')
        with self._connector.get_channel_stream('pty') as stream:
            self._test_stream(stream)


class TestTargetFragments(unittest.TestCase):
    """Tests for configurations using target fragments."""

    _config_templ: Dict[str, Any] = {
        'pw': {
            'pw_emu': {
                'target_files': [],
                'targets': {
                    'test-target': {
                        'test-key': 'test-value',
                    }
                },
            }
        }
    }

    _tf1_config: Dict[str, Any] = {
        'targets': {
            'test-target1': {},
            'test-target': {
                'test-key': 'test-value-file1',
            },
        }
    }

    _tf2_config: Dict[str, Any] = {'targets': {'test-target2': {}}}

    def setUp(self) -> None:
        with tempfile.NamedTemporaryFile(
            'wt', delete=False
        ) as config_file, tempfile.NamedTemporaryFile(
            'wt', delete=False
        ) as targets1_file, tempfile.NamedTemporaryFile(
            'wt', delete=False
        ) as targets2_file:
            self._config_path = config_file.name
            self._targets1_path = targets1_file.name
            self._targets2_path = targets2_file.name
            json.dump(self._tf1_config, targets1_file)
            json.dump(self._tf2_config, targets2_file)
            config = copy.deepcopy(self._config_templ)
            config['pw']['pw_emu']['target_files'].append(self._targets1_path)
            config['pw']['pw_emu']['target_files'].append(self._targets2_path)
            json.dump(config, config_file)
        self._config = Config(Path(self._config_path))

    def tearDown(self) -> None:
        os.unlink(self._config_path)
        os.unlink(self._targets1_path)
        os.unlink(self._targets2_path)

    def test_targets_loaded(self) -> None:
        self.assertIsNotNone(self._config.get(['targets', 'test-target']))
        self.assertIsNotNone(self._config.get(['targets', 'test-target1']))
        self.assertIsNotNone(self._config.get(['targets', 'test-target2']))

    def test_targets_priority(self) -> None:
        self.assertEqual(
            self._config.get(['targets', 'test-target', 'test-key']),
            'test-value',
        )


class TestHandles(unittest.TestCase):
    """Tests for Handles."""

    _config = {
        'emu': 'mock-emu',
        'config': 'test-config',
        'target': 'test-target',
        'gdb_cmd': ['test-gdb'],
        'channels': {
            'tcp_chan': {'type': 'tcp', 'host': 'localhost', 'port': 1234},
            'pty_chan': {'type': 'pty', 'path': 'path'},
        },
        'procs': {'proc0': {'pid': 1983}, 'proc1': {'pid': 1234}},
    }

    def test_serialize(self):
        handles = Handles('mock-emu', 'test-config')
        handles.add_channel_tcp('tcp_chan', 'localhost', 1234)
        handles.add_channel_pty('pty_chan', 'path')
        handles.add_proc('proc0', 1983)
        handles.add_proc('proc1', 1234)
        handles.set_target('test-target')
        handles.set_gdb_cmd(['test-gdb'])
        tmp = tempfile.TemporaryDirectory()
        handles.save(Path(tmp.name))
        with open(os.path.join(tmp.name, 'handles.json'), 'rt') as file:
            self.assertTrue(json.load(file) == self._config)
        tmp.cleanup()

    def test_load(self):
        tmp = tempfile.TemporaryDirectory()
        with open(os.path.join(tmp.name, 'handles.json'), 'wt') as file:
            json.dump(self._config, file)
        handles = Handles.load(Path(tmp.name))
        self.assertEqual(handles.emu, 'mock-emu')
        self.assertEqual(handles.gdb_cmd, ['test-gdb'])
        self.assertEqual(handles.target, 'test-target')
        self.assertEqual(handles.config, 'test-config')
        tmp.cleanup()


class TestConfig(ConfigHelper):
    """Stop tests for valid config."""

    _config: Dict[str, Any] = {
        'top': 'entry',
        'multi': {
            'level': {
                'entry': 0,
            },
        },
        'subst': 'a/$pw_env{PW_EMU_TEST_ENV_SUBST}/c',
        'targets': {
            'test-target': {
                'entry': [1, 2, 3],
                'mock-emu': {
                    'entry': 'test',
                },
            }
        },
        'mock-emu': {
            'executable': _mock_emu,
        },
        'list': ['a', '$pw_env{PW_EMU_TEST_ENV_SUBST}', 'c'],
        'bad-subst-type': '$pw_bad_subst_type{test}',
    }

    def setUp(self) -> None:
        super().setUp()
        self._cfg = Config(Path(self._config_file), 'test-target', 'mock-emu')

    def test_top_entry(self) -> None:
        self.assertEqual(self._cfg.get(['top']), 'entry')

    def test_empty_subst(self) -> None:
        with self.assertRaises(ConfigError):
            self._cfg.get(['subst'])

    def test_subst(self) -> None:
        with patch.dict('os.environ', {'PW_EMU_TEST_ENV_SUBST': 'b'}):
            self.assertEqual(self._cfg.get(['subst']), 'a/b/c')

    def test_multi_level_entry(self) -> None:
        self.assertEqual(self._cfg.get(['multi', 'level', 'entry']), 0)

    def test_get_target(self) -> None:
        self.assertEqual(self._cfg.get_targets(), ['test-target'])

    def test_target(self) -> None:
        self.assertEqual(self._cfg.get_target(['entry']), [1, 2, 3])

    def test_target_emu(self) -> None:
        self.assertEqual(self._cfg.get_target_emu(['entry']), 'test')

    def test_type_checking(self) -> None:
        with self.assertRaises(ConfigError):
            self._cfg.get(['top'], entry_type=int)
        self._cfg.get(['top'], entry_type=str)
        self._cfg.get_target(['entry'], entry_type=list)
        self._cfg.get_target_emu(['entry'], entry_type=str)
        self._cfg.get(['targets'], entry_type=dict)

    def test_non_optional(self) -> None:
        with self.assertRaises(ConfigError):
            self._cfg.get(['non-existing'], optional=False)

    def test_optional(self) -> None:
        self.assertEqual(self._cfg.get(['non-existing']), None)
        self.assertEqual(self._cfg.get(['non-existing'], entry_type=int), 0)
        self.assertEqual(self._cfg.get(['non-existing'], entry_type=str), '')
        self.assertEqual(self._cfg.get(['non-existing'], entry_type=list), [])

    def test_list(self) -> None:
        with self.assertRaises(ConfigError):
            self._cfg.get(['list'])
        with patch.dict('os.environ', {'PW_EMU_TEST_ENV_SUBST': 'b'}):
            self.assertEqual(self._cfg.get(['list']), ['a', 'b', 'c'])

    def test_bad_subst(self) -> None:
        with self.assertRaises(ConfigError):
            self._cfg.get(['bad-subst-type'])


if __name__ == '__main__':
    unittest.main()
