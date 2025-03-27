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
from typing import Iterable, NoReturn

from pw_bloat import bloat
from pw_bloat.label import DataSourceMap
from pw_bloat.label_output import BloatTableOutput
from pw_cli.color import colors
import pw_cli.log

_LOG = logging.getLogger(__name__)


def parse_args() -> argparse.Namespace:
    """Parse pw bloat arguments."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        'binary',
        help='Path to the ELF file to analyze',
        metavar='BINARY',
        type=Path,
    )
    parser.add_argument(
        '-c',
        '--custom-config',
        help=(
            'Use a provided Bloaty config instead of generating one '
            'from memory regions defined in the binary'
        ),
        type=Path,
    )
    parser.add_argument(
        '-d',
        '--data-sources',
        help='Comma-separated list of Bloaty data sources to report',
        type=lambda s: s.split(','),
    )
    parser.add_argument(
        '--diff',
        metavar='BINARY',
        help='Run a size diff against a base binary file',
        type=Path,
    )
    parser.add_argument(
        '-v',
        '--verbose',
        help=('Print all log messages ' '(only errors are printed by default)'),
        action='store_true',
    )

    return parser.parse_args()


def _run_size_report(
    elf: Path,
    custom_config: Path | None = None,
    data_sources: Iterable[str] = (),
) -> DataSourceMap:
    """Runs a size analysis on an ELF file, returning a pw_bloat size map.

    Returns:
        A size map of the labels in the binary under the provided data sources.

    Raises:
        NoMemoryRegions: The binary does not define bloat memory region symbols.
    """

    extra_args = ('--tsv',)

    if custom_config is not None:
        bloaty_tsv = bloat.basic_size_report(
            elf,
            custom_config,
            data_sources=data_sources,
            extra_args=extra_args,
        )
    else:
        bloaty_tsv = bloat.memory_regions_size_report(
            elf,
            data_sources=data_sources,
            extra_args=extra_args,
        )

    return DataSourceMap.from_bloaty_tsv(bloaty_tsv)


def _no_memory_regions_error(elf: Path) -> None:
    col = colors()

    _LOG.error('Executable file')
    _LOG.error('  %s', col.bold_white(elf))
    _LOG.error('does not define any bloat memory regions.')
    _LOG.error('')
    _LOG.error(
        (
            'Either provide a custom Bloaty configuration file via the '
            '%s option,'
        ),
        col.bold_white('--custom-config'),
    )
    _LOG.error(
        'or refer to https://pigweed.dev/pw_bloat/#memoryregions-data-source'
    )
    _LOG.error('for information on how to configure them.')


def _single_binary_report(
    elf: Path,
    custom_config: Path | None = None,
    data_sources: Iterable[str] = (),
) -> int:
    try:
        data_source_map = _run_size_report(elf, custom_config, data_sources)
    except bloat.NoMemoryRegions:
        _no_memory_regions_error(elf)
        return 1

    print(BloatTableOutput(data_source_map).create_table())
    return 0


def _diff_report(
    target: Path,
    base: Path,
    custom_config: Path | None = None,
    data_sources: Iterable[str] = (),
) -> int:
    try:
        base_map = _run_size_report(base, custom_config, data_sources)
        target_map = _run_size_report(target, custom_config, data_sources)
    except bloat.NoMemoryRegions as err:
        _no_memory_regions_error(err.elf)
        return 1

    diff = target_map.diff(base_map)

    print(BloatTableOutput(diff).create_table())
    return 0


def run_report(
    binary: Path | None = None,
    diff: Path | None = None,
    custom_config: Path | None = None,
    data_sources: Iterable[str] | None = None,
    verbose: bool = False,
) -> int:
    """Run binary size reports."""
    if binary is None:
        _LOG.error('the following arguments are required: BINARY')
        return -1

    if data_sources:
        # Use passed in data_sources
        pass
    elif custom_config is not None:
        # If a custom config is provided, automatic memoryregions are not being
        # used. Fallback to segments as the primary data source.
        data_sources = ('segments', 'sections')
    else:
        # No custom config will attempt to generate one from memoryregions.
        data_sources = ('memoryregions', 'sections')

    if not verbose:
        pw_cli.log.set_all_loggers_minimum_level(logging.ERROR)

    if diff is not None:
        return _diff_report(
            binary,
            diff,
            custom_config=custom_config,
            data_sources=data_sources,
        )

    return _single_binary_report(
        binary,
        custom_config=custom_config,
        data_sources=data_sources,
    )


def main() -> NoReturn:
    pw_cli.log.install()
    sys.exit(run_report(**vars(parse_args())))


if __name__ == '__main__':
    main()
