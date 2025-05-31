# Copyright 2025 The Pigweed Authors
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
"""Parses the version information out of FreeRTOS's task.h"""

import argparse
import re
import sys

_VERSION_MAJOR_REGEX = re.compile(
    r'^#define tskKERNEL_VERSION_MAJOR\s*(?P<value>[0-9]+)$'
)
_VERSION_MINOR_REGEX = re.compile(
    r'^#define tskKERNEL_VERSION_MINOR\s*(?P<value>[0-9]+)$'
)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('task_file', type=argparse.FileType('r'))
    return parser.parse_args()


def main(task_file) -> int:
    version: dict[str, str] = {}
    for line in task_file:
        if _VERSION_MAJOR_REGEX.match(line):
            version['MAJOR'] = _VERSION_MAJOR_REGEX.match(line)['value']
        if _VERSION_MINOR_REGEX.match(line):
            version['MINOR'] = _VERSION_MINOR_REGEX.match(line)['value']
        if len(version.keys()) == 2:
            break
    if len(version.keys()) != 2:
        print(
            'Failed to parse major/minor version from FreeRTOS include/task.h'
        )
        return 1

    for key, value in version.items():
        print(f'{key} = {value}')
    return 0


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
