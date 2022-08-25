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

from pw_bloat.label import from_bloaty_tsv
from pw_bloat.label_output import (BloatTableOutput, LineCharset, RstOutput,
                                   AsciiCharset)

_LOG = logging.getLogger(__name__)

MAX_COL_WIDTH = 50


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
    parser.add_argument('--data-sources',
                        type=delimited_list(';'),
                        help='List of sources to scan for')
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
                        type=delimited_list(';'),
                        help='Bloaty data source filter')
    parser.add_argument('--single-target',
                        type=str,
                        help='Single executable target')
    parser.add_argument('diff_targets',
                        type=delimited_list(';', 2),
                        nargs='*',
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

    default_bloaty = 'bloaty'
    bloaty_path = os.getenv('BLOATY_PATH', default_bloaty)

    # yapf: disable
    cmd = [
        bloaty_path,
        '-c', config,
        '-d', ','.join(data_sources),
        '--domain', 'vm',
        '-n', '0',
        filename,
        *extra_args
    ]

    # yapf: enable

    if base_file is not None:
        cmd.extend(['--', base_file])

    return subprocess.check_output(cmd)


def write_file(filename: str, contents: str, out_dir_file: str) -> None:
    path = os.path.join(out_dir_file, filename)
    with open(path, 'w') as output_file:
        output_file.write(contents)
    _LOG.debug('Output written to %s', path)


def single_target_output(target: str, bloaty_config: str, target_out_file: str,
                         out_dir: str, extra_args: Iterable[str]) -> int:

    try:
        single_output = run_bloaty(target,
                                   bloaty_config,
                                   data_sources=['segment_names', 'symbols'],
                                   extra_args=extra_args)

    except subprocess.CalledProcessError:
        _LOG.error('%s: failed to run size report on %s', sys.argv[0], target)
        return 1

    single_tsv = single_output.decode().splitlines()
    single_report = BloatTableOutput(from_bloaty_tsv(single_tsv),
                                     MAX_COL_WIDTH, LineCharset)

    rst_single_report = BloatTableOutput(from_bloaty_tsv(single_tsv),
                                         MAX_COL_WIDTH, AsciiCharset, True)

    single_report_table = single_report.create_table()

    print(single_report_table)
    write_file(target_out_file, rst_single_report.create_table(), out_dir)
    write_file(f'{target_out_file}.txt', single_report_table, out_dir)

    return 0


def main() -> int:
    """Program entry point."""

    args = parse_args()
    extra_args = ['--tsv']

    if args.single_target is not None:
        if args.source_filter:
            extra_args.extend(['--source-filter', args.source_filter])
        return single_target_output(args.single_target, args.bloaty_config[0],
                                    args.target, args.out_dir, extra_args)

    base_binaries: List[str] = []
    diff_binaries: List[str] = []

    try:
        for binary, base in args.diff_targets:
            diff_binaries.append(binary)
            base_binaries.append(base)

    except RuntimeError as err:
        _LOG.error('%s: %s', sys.argv[0], err)
        return 1

    data_sources = args.data_sources

    if args.data_sources is None:
        data_sources = ['segment_names', 'symbols']

    diff_report = ''
    rst_diff_report = ''

    for i, binary in enumerate(diff_binaries):
        if args.source_filter is not None and args.source_filter[i]:
            extra_args.extend(['--source-filter', args.source_filter[i]])
        try:
            single_output_base = run_bloaty(base_binaries[i],
                                            args.bloaty_config[i],
                                            data_sources=data_sources,
                                            extra_args=extra_args)

        except subprocess.CalledProcessError:
            _LOG.error('%s: failed to run base size report on %s', sys.argv[0],
                       base_binaries[i])
            return 1

        try:
            single_output_target = run_bloaty(binary,
                                              args.bloaty_config[i],
                                              data_sources=data_sources,
                                              extra_args=extra_args)

        except subprocess.CalledProcessError:
            _LOG.error('%s: failed to run target size report on %s',
                       sys.argv[0], binary)
            return 1

        if not single_output_target or not single_output_base:
            continue

        base_dsm = from_bloaty_tsv(single_output_base.decode().splitlines())
        target_dsm = from_bloaty_tsv(
            single_output_target.decode().splitlines())
        diff_dsm = target_dsm.diff(base_dsm)

        diff_report += BloatTableOutput(diff_dsm, MAX_COL_WIDTH,
                                        LineCharset).create_table()

        print(diff_report)
        curr_rst_report = RstOutput(diff_dsm, args.labels[i])
        if i == 0:
            rst_diff_report = curr_rst_report.create_table()
        else:
            rst_diff_report += f"{curr_rst_report.add_report_row()}\n"
        extra_args = ['--tsv']

    write_file(args.target, rst_diff_report, args.out_dir)
    write_file(f'{args.target}.txt', diff_report, args.out_dir)

    return 0


if __name__ == '__main__':
    pw_cli.log.install()
    sys.exit(main())
