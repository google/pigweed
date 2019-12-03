#!/usr/bin/env python3
# Copyright 2019 The Pigweed Authors
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
"""Installs or updates prebuilt tools.

Must be tested with Python 2 and Python 3.
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import tempfile


def parse(argv=None):
    """Parse arguments."""

    script_root = os.path.abspath(os.path.dirname(__file__))

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        '--install-dir', default=os.path.join(script_root, 'tools'))
    parser.add_argument(
        '--ensure-file', default=os.path.join(script_root, 'ensure_file'))
    parser.add_argument(
        '--cipd', default=os.path.join(script_root, 'cipd.py'))
    parser.add_argument(
        '--suppress-shell-commands', action='store_false',
        dest='print_shell_commands')

    return parser.parse_args(argv)


def update(argv=None):
    """Grab the tools listed in ensure_file."""

    args = parse(argv)

    cmd = [
        args.cipd,
        'ensure',
        '-ensure-file', args.ensure_file,
        '-root', args.install_dir,
        '-log-level', 'warning',
    ]  # yapf: disable

    os.environ['CIPD_PY_INSTALL_DIR'] = args.install_dir

    os.makedirs(args.install_dir, exist_ok=True)
    subprocess.check_call(cmd, stdout=sys.stderr)

    paths = [
        args.install_dir,
        os.path.join(args.install_dir, 'bin'),
    ]

    for path in paths:
        print('adding {} to path'.format(path), file=sys.stderr)

    paths.append(os.environ['PATH'])

    if args.print_shell_commands:
        with tempfile.NamedTemporaryFile(mode='w', delete=False,
                                         prefix='cipdsetup') as temp:
            print('PATH="{}"'.format(os.pathsep.join(paths)), file=temp)
            print('export PATH', file=temp)
            print('CIPD_INSTALL_DIR="{}"'.format(args.install_dir), file=temp)
            print('export CIPD_INSTALL_DIR', file=temp)

            print('. {}'.format(temp.name))


if __name__ == '__main__':
    update()
    sys.exit(0)
