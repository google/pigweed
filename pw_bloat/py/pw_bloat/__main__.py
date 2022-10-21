# Copyright 2022 The Pigweed Authors
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
"""Size reporting utilities."""

import argparse
import logging
from pathlib import Path
import sys

from pw_bloat import bloat
from pw_bloat.label import DataSourceMap
from pw_bloat.label_output import BloatTableOutput
import pw_cli.log

_LOG = logging.getLogger(__name__)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('binary',
                        help='Path to the ELF file to analyze',
                        type=Path)
    parser.add_argument(
        '-d',
        '--data-sources',
        help='Comma-separated list of additional Bloaty data sources to report',
        type=lambda s: s.split(','),
        default=())
    parser.add_argument('-v',
                        '--verbose',
                        help=('Print all log messages '
                              '(only errors are printed by default)'),
                        action='store_true')

    return parser.parse_args()


def main() -> int:
    """Run binary size reports."""

    args = _parse_args()

    if not args.verbose:
        pw_cli.log.set_all_loggers_minimum_level(logging.ERROR)

    try:
        bloaty_tsv = bloat.memory_regions_size_report(
            args.binary,
            additional_data_sources=args.data_sources,
            extra_args=('--tsv', ))
    except bloat.NoMemoryRegions:
        _LOG.error('Executable %s does not define any bloat memory regions',
                   args.binary)
        _LOG.error(
            'Refer to https://pigweed.dev/pw_bloat/#memoryregions-data-source')
        _LOG.error('for information on how to configure them.')
        return 1

    data_source_map = DataSourceMap.from_bloaty_tsv(bloaty_tsv)

    print(BloatTableOutput(data_source_map).create_table())
    return 0


if __name__ == '__main__':
    sys.exit(main())
