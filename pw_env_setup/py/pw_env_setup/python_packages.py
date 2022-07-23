#!/usr/bin/env python

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
"""Save list of installed packages and versions."""

import argparse
import subprocess
import sys
from typing import Dict, List, TextIO, Union


def _installed_packages():
    """Run pip python_packages and write to out."""
    cmd = [
        'python',
        '-m',
        'pip',
        'freeze',
        '--exclude-editable',
        '--local',
    ]
    proc = subprocess.run(cmd, capture_output=True)
    for line in proc.stdout.decode().splitlines():
        if ' @ ' not in line:
            yield line


def ls(out: TextIO) -> int:  # pylint: disable=invalid-name
    """Run pip python_packages and write to out."""
    for package in _installed_packages():
        print(package, file=out)

    return 0


class UpdateRequiredError(Exception):
    pass


def _stderr(*args, **kwargs):
    return print(*args, file=sys.stderr, **kwargs)


def diff(expected: TextIO) -> int:
    """Report on differences between installed and expected versions."""
    actual_lines = set(_installed_packages())
    expected_lines = set(expected.read().splitlines())

    if actual_lines == expected_lines:
        _stderr('package versions are identical')
        return 0

    removed_entries: Dict[str, str] = dict(
        x.split('==', 1)  # type: ignore[misc]
        for x in expected_lines - actual_lines)
    added_entries: Dict[str, str] = dict(
        x.split('==', 1)  # type: ignore[misc]
        for x in actual_lines - expected_lines)

    new_packages = set(added_entries) - set(removed_entries)
    removed_packages = set(removed_entries) - set(added_entries)
    updated_packages = set(added_entries).intersection(set(removed_entries))

    if removed_packages:
        _stderr('Removed packages')
        for package in removed_packages:
            _stderr(f'  {package}=={removed_entries[package]}')

    if updated_packages:
        _stderr('Updated packages')
        for package in updated_packages:
            _stderr(f'  {package}=={added_entries[package]} (from '
                    f'{removed_entries[package]})')

    if new_packages:
        _stderr('New packages')
        for package in new_packages:
            _stderr(f'  {package}=={added_entries[package]}')

    if updated_packages or new_packages:
        _stderr("Package versions don't match!")
        _stderr(f"""
Please do the following:

* purge your environment directory
  * Linux/Mac: 'rm -rf "$_PW_ACTUAL_ENVIRONMENT_ROOT"'
  * Windows: 'rmdir /S %_PW_ACTUAL_ENVIRONMENT_ROOT%'
* bootstrap
  * Linux/Mac: '. ./bootstrap.sh'
  * Windows: 'bootstrap.bat'
* update the constraint file
  * 'pw python-packages list {expected.name}'
""")
        return -1

    return 0


def parse(argv: Union[List[str], None] = None) -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='cmd')

    list_parser = subparsers.add_parser(
        'list', aliases=('ls', ), help='List installed package versions.')
    list_parser.add_argument('out',
                             type=argparse.FileType('w'),
                             default=sys.stdout,
                             nargs='?')

    diff_parser = subparsers.add_parser(
        'diff',
        help='Show differences between expected and actual package versions.',
    )
    diff_parser.add_argument('expected', type=argparse.FileType('r'))

    return parser.parse_args(argv)


def main() -> int:
    try:
        args = vars(parse())
        cmd = args.pop('cmd')
        if cmd == 'diff':
            return diff(**args)
        if cmd == 'list':
            return ls(**args)
        return -1
    except subprocess.CalledProcessError as err:
        print(file=sys.stderr)
        print(err.output, file=sys.stderr)
        raise


if __name__ == '__main__':
    sys.exit(main())
