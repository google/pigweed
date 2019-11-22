#!/usr/bin/env python3
# Copyright 2019 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy
# of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
"""Installs or updates prebuilt tools.

Must be tested with Python 2 and Python 3.
"""

from __future__ import print_function

import os
import subprocess
import sys
import tempfile


def update():
    """Grab the tools listed in ensure_file."""

    script_root = os.path.abspath(os.path.dirname(__file__))

    cmd = [
        os.path.join(script_root, 'cipd.py'),
        'ensure',
        '-ensure-file', os.path.join(script_root, 'ensure_file'),
        '-root', script_root,
        '-log-level', 'warning',
    ]  # yapf: disable

    subprocess.check_call(cmd, stdout=sys.stderr)

    paths = [
        '{}/tools'.format(script_root),
        '{}/tools/bin'.format(script_root),
    ]

    for path in paths:
        print('adding {} to path'.format(path), file=sys.stderr)

    paths.append(os.environ['PATH'])

    with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp:
        print('PATH="{}"'.format(os.pathsep.join(paths)), file=temp)
        print('export PATH', file=temp)

        print('. {}'.format(temp.name))


if __name__ == '__main__':
    update()
    sys.exit(0)
