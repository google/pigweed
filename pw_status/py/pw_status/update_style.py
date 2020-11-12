#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Updates pw::Status usages from Status::CODE to Status::Code() style.

Also updates StatusWithSize.
"""

import argparse
from pathlib import Path
import re
import sys
from typing import Iterable

from pw_presubmit import git_repo

_REMAP = {
    'OK': 'Ok',
    'CANCELLED': 'Cancelled',
    'UNKNOWN': 'Unknown',
    'INVALID_ARGUMENT': 'InvalidArgument',
    'DEADLINE_EXCEEDED': 'DeadlineExceeded',
    'NOT_FOUND': 'NotFound',
    'ALREADY_EXISTS': 'AlreadyExists',
    'PERMISSION_DENIED': 'PermissionDenied',
    'UNAUTHENTICATED': 'Unauthenticated',
    'RESOURCE_EXHAUSTED': 'ResourceExhausted',
    'FAILED_PRECONDITION': 'FailedPrecondition',
    'ABORTED': 'Aborted',
    'OUT_OF_RANGE': 'OutOfRange',
    'UNIMPLEMENTED': 'Unimplemented',
    'INTERNAL': 'Internal',
    'UNAVAILABLE': 'Unavailable',
    'DATA_LOSS': 'DataLoss',
}

_CODES = '|'.join(_REMAP.keys())
_FUNCTIONS = '|'.join(_REMAP.values())

_STATUS_WITH_SIZE_CTOR = re.compile(
    fr'\bStatusWithSize\(Status::({_CODES}),\s*'.encode())
_STATUS = re.compile(fr'\b(Status|StatusWithSize)::({_CODES})(?!")\b'.encode())
_STATUS_EQUALITY = re.compile(
    fr'Status::(?P<l_func>{_FUNCTIONS})\(\)\s+==\s+(?P<value>[a-zA-Z0-9_.()]+)|'
    fr'\s+==\s+(?:pw::)?Status::(?P<r_func>{_FUNCTIONS})\(\)'.encode())


def _remap_status_with_size(match) -> bytes:
    return f'StatusWithSize::{_REMAP[match.group(1).decode()]}('.encode()


def _remap_codes(match) -> bytes:
    status, code = (g.decode() for g in match.groups())
    return f'{status}::{_REMAP[code]}()'.encode()


def _remap_equality(match) -> bytes:
    l_func, status, r_func = (g.decode() for g in match.groups(b''))
    func = l_func or r_func
    return (f'{status}.ok()'
            if func == 'Ok' else f'{status}.Is{func}()').encode()


def _parse_args():
    """Parses and return command line arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths',
                        nargs='*',
                        type=Path,
                        help='Paths to repositories')
    return parser.parse_args()


def update_status(paths: Iterable[Path]) -> None:
    if not paths:
        paths = [Path.cwd()]

    for path in paths:
        if git_repo.has_uncommitted_changes(path):
            raise RuntimeError('There are pending changes in the Git repo!')

        updated = 0

        for file in git_repo.list_files(pathspecs=('*.h', '*.cc', '*.cpp'),
                                        repo_path=path):
            orig = file.read_bytes()

            # Replace StatusWithSize constructor
            text = _STATUS_WITH_SIZE_CTOR.sub(_remap_status_with_size, orig)

            # Replace Status and StatusWithSize
            text = _STATUS.sub(_remap_codes, text)

            text = _STATUS_EQUALITY.sub(_remap_equality, text)

            if orig != text:
                updated += 1
                file.write_bytes(text)

    print('Updated', updated, 'files.')
    print('Manually inspect the changes! This script is not perfect.')


def main():
    return update_status(**vars(_parse_args()))


if __name__ == '__main__':
    sys.exit(main())
