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
"""renode emulator tests."""

import json
import os
import sys
import struct
import tempfile
import time
import unittest

from pathlib import Path
from typing import Any, Dict, Optional

from pw_emu.core import InvalidChannelName, InvalidChannelType
from tests.common import check_prog, ConfigHelperWithEmulator


# TODO: b/301382004 - The Python Pigweed package install (into python-venv)
# races with running this test and there is no way to add that package as a test
# depedency without creating circular depedencies. This means we can't rely on
# using Pigweed tools like pw cli or the arm-none-eabi-gdb wrapper.
#
# run the arm_gdb.py wrapper directly
_arm_none_eabi_gdb_path = Path(
    os.path.join(
        os.environ['PW_ROOT'],
        'pw_env_setup',
        'py',
        'pw_env_setup',
        'entry_points',
        'arm_gdb.py',
    )
).resolve()


class TestRenode(ConfigHelperWithEmulator):
    """Tests for a valid renode configuration."""

    _config = {
        'gdb': ['python', str(_arm_none_eabi_gdb_path)],
        'renode': {
            'executable': 'renode',
        },
        'targets': {
            'test-target': {
                'renode': {
                    'machine': 'platforms/boards/stm32f4_discovery-kit.repl',
                }
            }
        },
    }

    def setUp(self) -> None:
        super().setUp()
        # no image to run so start paused
        self._emu.start(target='test-target', pause=True)

    def tearDown(self) -> None:
        self._emu.stop()
        super().tearDown()

    def test_running(self) -> None:
        self.assertTrue(self._emu.running())

    def test_list_properties(self) -> None:
        self.assertIsNotNone(self._emu.list_properties('sysbus.usart1'))

    def test_get_property(self) -> None:
        self.assertIsNotNone(
            self._emu.get_property('sysbus.usart1', 'BaudRate')
        )

    def test_set_property(self) -> None:
        self._emu.set_property('sysbus.timer1', 'Frequency', 100)
        self.assertEqual(
            int(self._emu.get_property('sysbus.timer1', 'Frequency'), 16), 100
        )


class TestRenodeInvalidChannelType(ConfigHelperWithEmulator):
    """Test invalid channel type configuration."""

    _config = {
        'renode': {
            'executable': 'renode',
        },
        'targets': {
            'test-target': {
                'renode': {
                    'machine': 'platforms/boards/stm32f4_discovery-kit.repl',
                    'channels': {
                        'terminals': {
                            'test_uart': {
                                'device-path': 'sysbus.usart1',
                                'type': 'invalid',
                            }
                        }
                    },
                }
            }
        },
    }

    def test_start(self) -> None:
        with self.assertRaises(InvalidChannelType):
            self._emu.start('test-target', pause=True)


class TestRenodeChannels(ConfigHelperWithEmulator):
    """Tests for a valid renode channels configuration."""

    _config = {
        'gdb': ['python', str(_arm_none_eabi_gdb_path)],
        'renode': {
            'executable': 'renode',
        },
        'targets': {
            'test-target': {
                'renode': {
                    'machine': 'platforms/boards/stm32f4_discovery-kit.repl',
                    'channels': {
                        'terminals': {
                            'test_uart': {
                                'device-path': 'sysbus.usart1',
                            }
                        }
                    },
                }
            }
        },
    }

    def setUp(self) -> None:
        super().setUp()
        # no image to run so start paused
        self._emu.start(target='test-target', pause=True)

    def tearDown(self) -> None:
        self._emu.stop()
        super().tearDown()

    def test_bad_channel_name(self) -> None:
        with self.assertRaises(InvalidChannelName):
            self._emu.get_channel_addr('serial1')

    def set_reg(self, addr: int, val: int) -> None:
        self._emu.run_gdb_cmds([f'set *(unsigned int*){addr}={val}'])

    def get_reg(self, addr: int) -> int:
        temp = tempfile.NamedTemporaryFile(delete=False)
        temp.close()

        res = self._emu.run_gdb_cmds(
            [
                f'dump val {temp.name} *(char*){addr}',
                'disconnect',
            ]
        )
        self.assertEqual(res.returncode, 0, res.stderr.decode('ascii'))

        with open(temp.name, 'rb') as file:
            ret = file.read(1)

        self.assertNotEqual(ret, b'', res.stderr.decode('ascii'))

        os.unlink(temp.name)

        return struct.unpack('B', ret)[0]

    def poll_data(self, timeout: int) -> Optional[int]:
        usart_sr = 0x40011000
        usart_dr = 0x40011004
        deadline = time.monotonic() + timeout
        while self.get_reg(usart_sr) & 0x20 == 0:
            time.sleep(0.1)
            if time.monotonic() > deadline:
                return None
        return self.get_reg(usart_dr)

    def test_channel_stream(self) -> None:
        ok, msg = check_prog('arm-none-eabi-gdb')
        if not ok:
            self.skipTest(msg)

        usart_cr1 = 0x4001100C
        # enable RX and TX
        self.set_reg(usart_cr1, 0xC)

        stream = self._emu.get_channel_stream('test_uart')
        stream.write('test\n'.encode('ascii'))

        self.assertEqual(self.poll_data(5), ord('t'))
        self.assertEqual(self.poll_data(5), ord('e'))
        self.assertEqual(self.poll_data(5), ord('s'))
        self.assertEqual(self.poll_data(5), ord('t'))


class TestRenodeChannelsPty(TestRenodeChannels):
    """Tests for configurations using PTY channels."""

    _config: Dict[str, Any] = {}
    _config.update(json.loads(json.dumps(TestRenodeChannels._config)))
    _config['renode']['channels'] = {'terminals': {'type': 'pty'}}

    def setUp(self):
        if sys.platform == 'win32':
            self.skipTest('pty not supported on win32')
        super().setUp()

    def test_get_path(self) -> None:
        self.assertTrue(os.path.exists(self._emu.get_channel_path('test_uart')))


def main() -> None:
    ok, msg = check_prog('renode')
    if not ok:
        print(f'skipping tests: {msg}')
        sys.exit(0)

    unittest.main()


if __name__ == '__main__':
    main()
