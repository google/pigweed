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
"""Generate a coverage report using llvm-cov."""

import argparse
import json
import logging
import sys
import subprocess
from pathlib import Path
from typing import List, Dict, Any

_LOG = logging.getLogger(__name__)


def _parser_args() -> Dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--llvm-cov-path',
        type=Path,
        required=True,
        help='Path to the llvm-cov binary to use to generate coverage reports.',
    )
    parser.add_argument(
        '--format',
        dest='format_type',
        type=str,
        choices=['text', 'html', 'lcov', 'json'],
        required=True,
        help='Desired output format of the code coverage report.',
    )
    parser.add_argument(
        '--test-metadata-path',
        type=Path,
        required=True,
        help='Path to the *.test_metadata.json file that describes all of the '
        'tests being used to generate a coverage report.',
    )
    parser.add_argument(
        '--profdata-path',
        type=Path,
        required=True,
        help='Path for the output merged profdata file to use with generating a'
        ' coverage report for the tests described in --test-metadata.',
    )
    parser.add_argument(
        '--root-dir',
        type=Path,
        required=True,
        help='Path to the project\'s root directory.',
    )
    parser.add_argument(
        '--build-dir',
        type=Path,
        required=True,
        help='Path to the ninja build directory.',
    )
    parser.add_argument(
        '--output-dir',
        type=Path,
        required=True,
        help='Path to where the output report should be placed. This must be a '
        'relative path (from the current working directory) to ensure the '
        'depfiles are generated correctly.',
    )
    parser.add_argument(
        '--depfile-path',
        type=Path,
        required=True,
        help='Path for the output depfile to convey the extra input '
        'requirements from parsing --test-metadata.',
    )
    parser.add_argument(
        '--filter-path',
        dest='filter_paths',
        type=str,
        action='append',
        default=[],
        help='Only these folder paths or files will be included in the output. '
        'To work properly, these must be aboslute paths or relative paths from '
        'the current working directory. No globs or regular expression features'
        ' are supported.',
    )
    parser.add_argument(
        '--ignore-filename-pattern',
        dest='ignore_filename_patterns',
        type=str,
        action='append',
        default=[],
        help='Any file path that matches one of these regular expression '
        'patterns will be excluded from the output report (possibly even if '
        'that path was included in --filter-paths). The regular expression '
        'engine for these is somewhat primitive and does not support things '
        'like look-ahead or look-behind.',
    )
    return vars(parser.parse_args())


def generate_report(
    llvm_cov_path: Path,
    format_type: str,
    test_metadata_path: Path,
    profdata_path: Path,
    root_dir: Path,
    build_dir: Path,
    output_dir: Path,
    depfile_path: Path,
    filter_paths: List[str],
    ignore_filename_patterns: List[str],
) -> int:
    """Generate a coverage report using llvm-cov."""

    # Ensure directories that need to be absolute are.
    root_dir = root_dir.resolve()
    build_dir = build_dir.resolve()

    # Open the test_metadata_path, parse it to JSON, and extract out the
    # test binaries.
    test_metadata = json.loads(test_metadata_path.read_text())
    test_binaries = [
        Path(obj['test_directory']) / obj['test_name']
        for obj in test_metadata
        if 'test_type' in obj and obj['test_type'] == 'unit_test'
    ]

    # llvm-cov export does not create an output file, so we mimic it by creating
    # the directory structure and writing to file outself after we run the
    # command.
    if format_type in ['lcov', 'json']:
        export_output_path = (
            output_dir / 'report.lcov'
            if format_type == 'lcov'
            else output_dir / 'report.json'
        )
        output_dir.mkdir(parents=True, exist_ok=True)

    # Build the command to the llvm-cov subtool based on provided arguments.
    command = [str(llvm_cov_path)]
    if format_type in ['html', 'text']:
        command += [
            'show',
            '--format',
            format_type,
            '--output-dir',
            str(output_dir),
        ]
    else:  # format_type in ['lcov', 'json']
        command += [
            'export',
            '--format',
            # `text` is JSON format when using `llvm-cov`.
            format_type if format_type == 'lcov' else 'text',
        ]
    # We really need two `--path-equivalence` options to be able to map both the
    # root directory for coverage files to the absolute path of the project
    # root_dir and to be able to map "out/" prefix to the provided build_dir.
    #
    # llvm-cov does not currently support two `--path-equivalence` options, so
    # we use `--compilation-dir` and `--path-equivalence` together. This has the
    # unfortunate consequence of showing file paths as absolute in the JSON,
    # LCOV, and text reports.
    #
    # An unwritten assumption here is that root_dir must be an
    # absolute path to enable file-path-based filtering.
    #
    # This is due to turning all file paths into absolute files here:
    # https://github.com/llvm-mirror/llvm/blob/2c4ca6832fa6b306ee6a7010bfb80a3f2596f824/tools/llvm-cov/CodeCoverage.cpp#L188.
    command += [
        '--compilation-dir',
        str(root_dir),
    ]
    # Pigweed maps any build directory to out, which causes generated files to
    # be reported to exist under the out directory, which may not exist if the
    # build directory is not exactly out. This maps out back to the build
    # directory so generated files can be found.
    command += [
        '--path-equivalence',
        f'{str(root_dir)}/out,{str(build_dir)}',
    ]
    command += [
        '--instr-profile',
        str(profdata_path),
    ]
    command += [
        f'--ignore-filename-regex={path}' for path in ignore_filename_patterns
    ]
    # The test binary positional argument MUST appear before the filter path
    # positional arguments. llvm-cov is a horrible interface.
    command += [str(test_binaries[0])]
    command += [f'--object={binary}' for binary in test_binaries[1:]]
    command += [
        str(Path(filter_path).resolve()) for filter_path in filter_paths
    ]

    _LOG.info('')
    _LOG.info(' '.join(command))
    _LOG.info('')

    # Generate the coverage report by invoking the command.
    if format_type in ['html', 'text']:
        output = subprocess.run(command)
        if output.returncode != 0:
            return output.returncode
    else:  # format_type in ['lcov', 'json']
        output = subprocess.run(command, capture_output=True)
        if output.returncode != 0:
            _LOG.error(output.stderr)
            return output.returncode
        export_output_path.write_bytes(output.stdout)

    # Generate the depfile that describes the dependency on the test binaries
    # used to create the report output.
    depfile_target = Path('.')
    if format_type in ['lcov', 'json']:
        depfile_target = export_output_path
    elif format_type == 'text':
        depfile_target = output_dir / 'index.txt'
    else:  # format_type == 'html'
        depfile_target = output_dir / 'index.html'
    depfile_path.write_text(
        ''.join(
            [
                str(depfile_target),
                ': \\\n',
                *[str(binary) + ' \\\n' for binary in test_binaries],
            ]
        )
    )

    return 0


def main() -> int:
    return generate_report(**_parser_args())


if __name__ == "__main__":
    sys.exit(main())
