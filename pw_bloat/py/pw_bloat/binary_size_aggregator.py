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
"""
Collects binary size JSON outputs from bloat targets into a single file.
"""

import argparse
import json
import logging
from pathlib import Path
import sys

from typing import Dict

import pw_cli.log

_LOG = logging.getLogger(__package__)


def _parse_args() -> argparse.Namespace:
    """Parses the script's arguments."""

    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument(
        '--output',
        type=Path,
        required=True,
        help='Output JSON file',
    )
    parser.add_argument(
        'inputs',
        type=Path,
        nargs='+',
        help='Input JSON files',
    )

    return parser.parse_args()


def main(inputs: list[Path], output: Path) -> int:
    all_data: Dict[str, int] = {}

    for file in inputs:
        try:
            all_data |= json.loads(file.read_text())
        except FileNotFoundError:
            target_name = file.name.split('.')[0]
            _LOG.error('')
            _LOG.error('JSON input file %s does not exist', file)
            _LOG.error('')
            _LOG.error(
                'Check that the build target "%s" is a pw_size_report template',
                target_name,
            )
            _LOG.error('')
            return 1

    output.write_text(json.dumps(all_data, sort_keys=True, indent=2))
    return 0


if __name__ == '__main__':
    pw_cli.log.install()
    sys.exit(main(**vars(_parse_args())))
