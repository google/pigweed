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
"""Run doxygen on all Pigweed modules."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--gn-root',
        type=Path,
        required=True,
        help='Root of the GN tree.',
    )
    parser.add_argument(
        '--pigweed-modules-file',
        type=Path,
        required=True,
        help='Pigweed modules list file',
    )
    parser.add_argument(
        '--output-dir',
        type=Path,
        required=True,
        help='Location to write output to',
    )
    parser.add_argument(
        '--doxygen-config',
        type=Path,
        required=True,
        help='Location to write output to',
    )
    parser.add_argument(
        '--include-paths',
        nargs='+',
        type=Path,
    )
    return parser.parse_args()


def main(
    gn_root: Path,
    pigweed_modules_file: Path,
    output_dir: Path,
    doxygen_config: Path,
    include_paths: list[Path],
) -> None:
    root_build_dir = Path.cwd()

    # Pigweed top level module list.
    pw_module_list = [
        str((gn_root / path).resolve())
        for path in pigweed_modules_file.read_text().splitlines()
    ]

    # Use selected modules only if provided
    if include_paths:
        pw_module_list = [
            str((root_build_dir / path).resolve()) for path in include_paths
        ]

    env = os.environ.copy()
    env['PW_DOXYGEN_OUTPUT_DIRECTORY'] = str(output_dir.resolve())
    env['PW_DOXYGEN_INPUT'] = ' '.join(pw_module_list)
    env['PW_DOXYGEN_PROJECT_NAME'] = 'Pigweed'

    # Clean out old xmls
    shutil.rmtree(output_dir, ignore_errors=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    command = ['doxygen', str(doxygen_config.resolve())]
    process = subprocess.run(command, env=env, check=True)
    sys.exit(process.returncode)


if __name__ == '__main__':
    main(**vars(_parse_args()))
