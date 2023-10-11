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
"""Launcher and Connector for mock emulator"""

import json
import os
from pathlib import Path
from typing import Any, Optional, List, Union
import time

from pw_emu.core import (
    Connector,
    Launcher,
    InvalidProperty,
    InvalidPropertyPath,
)

# mock emulator script
_mock_emu = [
    'python',
    os.path.join(Path(os.path.dirname(__file__)).resolve(), 'mock_emu.py'),
]


def wait_for_file_size(
    path: Union[os.PathLike, str], size: int, timeout: int = 5
) -> None:
    deadline = time.monotonic() + timeout
    while not os.path.exists(path):
        if time.monotonic() > deadline:
            break
        time.sleep(0.1)

    while os.path.getsize(path) < size:
        if time.monotonic() > deadline:
            break
        time.sleep(0.1)


class MockEmuLauncher(Launcher):
    """Launcher for mock emulator"""

    def __init__(
        self,
        config_path: Path,
    ):
        super().__init__('mock-emu', config_path)
        self._wdir: Optional[Path] = None
        self.log = True

    def _pre_start(
        self,
        target: str,
        file: Optional[Path] = None,
        pause: bool = False,
        debug: bool = False,
        args: Optional[str] = None,
    ) -> List[str]:
        channels = []
        if self._config.get_target(['pre-start-cmds']):
            self._handles.add_channel_tcp('test_subst_tcp', 'localhost', 1234)
            self._handles.add_channel_pty('test_subst_pty', 'pty-path')
        if self._config.get_emu(['gdb_channel']):
            channels += ['--tcp-channel', 'gdb']
        if self._config.get_emu(['tcp_channel']):
            channels += ['--tcp-channel', 'tcp']
        if self._config.get_emu(['pty_channel']):
            channels += ['--pty-channel', 'pty']
        if len(channels) > 0:
            channels += ['--working-dir', str(self._wdir)]
        return _mock_emu + channels + ['starting mock emulator']

    def _post_start(self) -> None:
        if not self._wdir:
            return

        if self._config.get_emu(['gdb_channel']):
            path = os.path.join(self._wdir, 'gdb')
            wait_for_file_size(path, 5, 5)
            with open(path, 'r') as file:
                port = int(file.read())
            self._handles.add_channel_tcp('gdb', 'localhost', port)

        if self._config.get_emu(['tcp_channel']):
            path = os.path.join(self._wdir, 'tcp')
            wait_for_file_size(path, 5, 5)
            with open(path, 'r') as file:
                port = int(file.read())
            self._handles.add_channel_tcp('tcp', 'localhost', port)

        if self._config.get_emu(['pty_channel']):
            path = os.path.join(self._wdir, 'pty')
            wait_for_file_size(path, 5, 5)
            with open(path, 'r') as file:
                pty_path = file.read()
            self._handles.add_channel_pty('pty', pty_path)

    def _get_connector(self, wdir: Path) -> Connector:
        return MockEmuConnector(wdir)


class MockEmuConnector(Connector):
    """Connector for mock emulator"""

    _props = {
        'path1': {
            'prop1': 'val1',
        }
    }

    def reset(self) -> None:
        Path(os.path.join(self._wdir, 'reset')).touch()

    def cont(self) -> None:
        Path(os.path.join(self._wdir, 'cont')).touch()

    def list_properties(self, path: str) -> List[Any]:
        try:
            return list(self._props[path].keys())
        except KeyError:
            raise InvalidPropertyPath(path)

    def set_property(self, path: str, prop: str, value: str) -> None:
        if not self._props.get(path):
            raise InvalidPropertyPath(path)
        if not self._props[path].get(prop):
            raise InvalidProperty(path, prop)
        self._props[path][prop] = value
        with open(os.path.join(self._wdir, 'props.json'), 'w') as file:
            json.dump(self._props, file)

    def get_property(self, path: str, prop: str) -> Any:
        try:
            with open(os.path.join(self._wdir, 'props.json'), 'r') as file:
                self._props = json.load(file)
        except OSError:
            pass
        if not self._props.get(path):
            raise InvalidPropertyPath(path)
        if not self._props[path].get(prop):
            raise InvalidProperty(path, prop)
        return self._props[path][prop]
