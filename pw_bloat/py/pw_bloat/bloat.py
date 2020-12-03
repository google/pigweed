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
"""
bloat is a script which generates a size report card for binary files.
"""

import argparse
import logging
import os
import subprocess
import sys
from typing import List, Iterable, Optional

import pw_cli.log

from pw_bloat.binary_diff import BinaryDiff
from pw_bloat import bloat_output

_LOG = logging.getLogger(__name__)


def parse_args() -> argparse.Namespace:
    """Parses the script's arguments."""
    def delimited_list(delimiter: str, items: Optional[int] = None):
        def _parser(arg: str):
            args = arg.split(delimiter)

            if items and len(args) != items:
                raise argparse.ArgumentTypeError(
                    'Argument must be a '
                    f'{delimiter}-delimited list with {items} items: "{arg}"')

            return args

        return _parser

    parser = argparse.ArgumentParser(
        'Generate a size report card for binaries')
    parser.add_argument('--bloaty-config',
                        type=delimited_list(';'),
                        required=True,
                        help='Data source configuration for Bloaty')
    parser.add_argument('--full',
                        action='store_true',
                        help='Display full bloat breakdown by symbol')
    parser.add_argument('--labels',
                        type=delimited_list(';'),
                        default='',
                        help='Labels for output binaries')
    parser.add_argument('--out-dir',
                        type=str,
                        required=True,
                        help='Directory in which to write output files')
    parser.add_argument('--target',
                        type=str,
                        required=True,
                        help='Build target name')
    parser.add_argument('--title',
                        type=str,
                        default='pw_bloat',
                        help='Report title')
    parser.add_argument('--source-filter',
                        type=str,
                        help='Bloaty data source filter')
    parser.add_argument('diff_targets',
                        type=delimited_list(';', 2),
                        nargs='+',
                        metavar='DIFF_TARGET',
                        help='Binary;base pairs to process')

    return parser.parse_args()


def run_bloaty(
    filename: str,
    config: str,
    base_file: Optional[str] = None,
    data_sources: Iterable[str] = (),
    extra_args: Iterable[str] = ()
) -> bytes:
    """Executes a Bloaty size report on some binary file(s).

    Args:
        filename: Path to the binary.
        config: Path to Bloaty config file.
        base_file: Path to a base binary. If provided, a size diff is performed.
        data_sources: List of Bloaty data sources for the report.
        extra_args: Additional command-line arguments to pass to Bloaty.

    Returns:
        Binary output of the Bloaty invocation.

    Raises:
        subprocess.CalledProcessError: The Bloaty invocation failed.
    """

    # TODO(frolv): Point the default bloaty path to a prebuilt in Pigweed.
    default_bloaty = 'bloaty'
    bloaty_path = os.getenv('BLOATY_PATH', default_bloaty)

    # yapf: disable
    cmd = [
        bloaty_path,
        '-c', config,
        '-d', ','.join(data_sources),
        '--domain', 'vm',
        filename,
        *extra_args
    ]
    # yapf: enable

    if base_file is not None:
        cmd.extend(['--', base_file])

    return subprocess.check_output(cmd)


def main() -> int:
    """Program entry point."""

    args = parse_args()

    base_binaries: List[str] = []
    diff_binaries: List[str] = []

    try:
        for binary, base in args.diff_targets:
            diff_binaries.append(binary)
            base_binaries.append(base)
    except RuntimeError as err:
        _LOG.error('%s: %s', sys.argv[0], err)
        return 1

    data_sources = ['segment_names']
    if args.full:
        data_sources.append('fullsymbols')

    # TODO(frolv): CSV output is disabled for full reports as the default Bloaty
    # breakdown is printed. This script should be modified to print a custom
    # symbol breakdown in full reports.
    extra_args = [] if args.full else ['--csv']
    if args.source_filter:
        extra_args.extend(['--source-filter', args.source_filter])

    diffs: List[BinaryDiff] = []
    report = []

    for i, binary in enumerate(diff_binaries):
        binary_name = (args.labels[i]
                       if i < len(args.labels) else os.path.basename(binary))
        try:
            output = run_bloaty(binary, args.bloaty_config[i],
                                base_binaries[i], data_sources, extra_args)
            if not output:
                continue

            # TODO(frolv): Remove when custom output for full mode is added.
            if args.full:
                report.append(binary_name)
                report.append('-' * len(binary_name))
                report.append(output.decode())
                continue

            # Ignore the first row as it displays column names.
            bloaty_csv = output.decode().splitlines()[1:]
            diffs.append(BinaryDiff.from_csv(binary_name, bloaty_csv))
        except subprocess.CalledProcessError:
            _LOG.error('%s: failed to run diff on %s', sys.argv[0], binary)
            return 1

    def write_file(filename: str, contents: str) -> None:
        path = os.path.join(args.out_dir, filename)
        with open(path, 'w') as output_file:
            output_file.write(contents)
        _LOG.debug('Output written to %s', path)

    # TODO(frolv): Remove when custom output for full mode is added.
    if not args.full:
        out = bloat_output.TableOutput(args.title,
                                       diffs,
                                       charset=bloat_output.LineCharset)
        report.append(out.diff())

        rst = bloat_output.RstOutput(diffs)
        write_file(f'{args.target}', rst.diff())

    complete_output = '\n'.join(report) + '\n'
    write_file(f'{args.target}.txt', complete_output)
    print(complete_output)

    return 0


if __name__ == '__main__':
    pw_cli.log.install()
    sys.exit(main())
