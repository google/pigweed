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
"""
bloat is a script which generates a size report card for binary files.
"""

import argparse
import logging
import os
import subprocess
import sys
import json
from typing import Iterable, Optional
from pathlib import Path

import pw_cli.log

from pw_build.python_runner import expand_expressions, GnPaths
from pw_bloat.label import from_bloaty_tsv
from pw_bloat.label_output import (BloatTableOutput, LineCharset, RstOutput,
                                   AsciiCharset)

_LOG = logging.getLogger(__name__)

MAX_COL_WIDTH = 50


def parse_args() -> argparse.Namespace:
    """Parses the script's arguments."""

    parser = argparse.ArgumentParser(
        'Generate a size report card for binaries')
    parser.add_argument('--gn-arg-path',
                        type=str,
                        required=True,
                        help='File path to json of binaries')
    parser.add_argument('--single-report',
                        action="store_true",
                        help='Determine if calling single size report')

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
                         out_dir: str, data_sources: Iterable[str],
                         extra_args: Iterable[str]) -> int:

    try:
        single_output = run_bloaty(target,
                                   bloaty_config,
                                   data_sources=data_sources,
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


# TODO(frolv) Copied from python_runner.py
def _abspath(path: Path) -> Path:
    """Turns a path into an absolute path, not resolving symlinks."""
    return Path(os.path.abspath(path))


def _translate_file_paths(gn_arg_dict: dict, single_report: bool) -> dict:
    tool = gn_arg_dict['toolchain'] if gn_arg_dict['toolchain'] != gn_arg_dict[
        'default_toolchain'] else ''
    paths = GnPaths(root=_abspath(gn_arg_dict['root']),
                    build=_abspath(Path.cwd()),
                    toolchain=tool,
                    cwd=_abspath(gn_arg_dict['cwd']))
    for curr_arg in gn_arg_dict['binaries']:
        curr_arg['target'] = list(expand_expressions(paths,
                                                     curr_arg['target']))[0]
        if not single_report:
            curr_arg['base'] = list(expand_expressions(paths,
                                                       curr_arg['base']))[0]
    return gn_arg_dict


def main() -> int:
    """Program entry point."""

    args = parse_args()
    extra_args = ['--tsv']
    data_sources = ['segment_names', 'symbols']
    gn_arg_dict = {}
    json_file = open(args.gn_arg_path)
    gn_arg_dict = json.load(json_file)

    gn_arg_dict = _translate_file_paths(gn_arg_dict, args.single_report)

    if args.single_report:
        single_binary_args = gn_arg_dict['binaries'][0]
        if single_binary_args['source_filter']:
            extra_args.extend(
                ['--source-filter', single_binary_args['source_filter']])
        if single_binary_args['data_sources']:
            data_sources = single_binary_args['data_sources']

        return single_target_output(single_binary_args['target'],
                                    single_binary_args['bloaty_config'],
                                    gn_arg_dict['target_name'],
                                    gn_arg_dict['out_dir'], data_sources,
                                    extra_args)

    default_data_sources = ['segment_names', 'symbols']

    diff_report = ''
    rst_diff_report = ''
    for curr_diff_binary in gn_arg_dict['binaries']:

        curr_extra_args = extra_args.copy()
        data_sources = default_data_sources

        if curr_diff_binary['source_filter']:
            curr_extra_args.extend(
                ['--source-filter', curr_diff_binary['source_filter']])

        if curr_diff_binary['data_sources']:
            data_sources = curr_diff_binary['data_sources']

        try:
            single_output_base = run_bloaty(curr_diff_binary["base"],
                                            curr_diff_binary['bloaty_config'],
                                            data_sources=data_sources,
                                            extra_args=curr_extra_args)

        except subprocess.CalledProcessError:
            _LOG.error('%s: failed to run base size report on %s', sys.argv[0],
                       curr_diff_binary["base"])
            return 1

        try:
            single_output_target = run_bloaty(
                curr_diff_binary["target"],
                curr_diff_binary['bloaty_config'],
                data_sources=data_sources,
                extra_args=curr_extra_args)

        except subprocess.CalledProcessError:
            _LOG.error('%s: failed to run target size report on %s',
                       sys.argv[0], curr_diff_binary["target"])
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
        curr_rst_report = RstOutput(diff_dsm, curr_diff_binary['label'])
        if rst_diff_report == '':
            rst_diff_report = curr_rst_report.create_table()
        else:
            rst_diff_report += f"{curr_rst_report.add_report_row()}\n"

    write_file(gn_arg_dict['target_name'], rst_diff_report,
               gn_arg_dict['out_dir'])
    write_file(f"{gn_arg_dict['target_name']}.txt", diff_report,
               gn_arg_dict['out_dir'])

    return 0


if __name__ == '__main__':
    pw_cli.log.install()
    sys.exit(main())
