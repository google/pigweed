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
"""Common utilities for tests."""

import json
import os
import subprocess
import tempfile
import unittest

from pathlib import Path
from typing import Any, Optional, Dict

from pw_emu.frontend import Emulator


def check_prog(prog: str) -> tuple:
    msg = f'running {prog}'
    try:
        proc = subprocess.run([prog, '--help'], capture_output=True)
        if proc.returncode != 0:
            output = proc.stdout.decode('ascii') + proc.stderr.decode('ascii')
            msg = f'error {msg}: {output}'
            return (False, msg)
    except OSError as err:
        msg = f'error {msg}: {str(err)}'
        return (False, msg)
    return (True, msg)


class ConfigHelper(unittest.TestCase):
    """Helper that setups and tears down the configuration file"""

    _config: Optional[Dict[str, Any]] = None

    def setUp(self) -> None:
        self._wdir = tempfile.TemporaryDirectory()
        with tempfile.NamedTemporaryFile('wt', delete=False) as file:
            pw_emu_config: Dict[str, Any] = {'pw': {'pw_emu': {}}}
            if self._config:
                pw_emu_config['pw']['pw_emu'].update(self._config)
            json.dump(pw_emu_config, file)
            self._config_file = file.name

    def tearDown(self) -> None:
        self._wdir.cleanup()
        os.unlink(self._config_file)


class ConfigHelperWithEmulator(ConfigHelper):
    """Helper that setups and tears down the configuration file"""

    def setUp(self) -> None:
        super().setUp()
        self._emu = Emulator(Path(self._wdir.name), Path(self._config_file))
