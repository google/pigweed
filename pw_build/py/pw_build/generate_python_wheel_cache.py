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
"""Download all Python packages required for a pw_python_venv."""

import argparse
from pathlib import Path
import subprocess
import sys
from typing import List


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--pip-download-log',
        type=Path,
        required=True,
        help='Path to the root gn build dir.',
    )
    parser.add_argument(
        '--wheel-dir',
        type=Path,
        required=True,
        help='Path to save wheel files.',
    )
    parser.add_argument(
        '-r',
        '--requirement',
        type=Path,
        nargs='+',
        required=True,
        help='Requirements files',
    )
    return parser.parse_args()


def main(
    pip_download_log: Path,
    wheel_dir: Path,
    requirement: List[Path],
) -> int:
    """Download all Python packages required for a pw_python_venv."""

    # Delete existing wheels from the out dir, there may be stale versions.
    # shutil.rmtree(wheel_dir, ignore_errors=True)
    wheel_dir.mkdir(parents=True, exist_ok=True)

    pip_download_args = [
        sys.executable,
        "-m",
        "pip",
        "--log",
        str(pip_download_log),
        "download",
        "--dest",
        str(wheel_dir),
    ]
    for req in requirement:
        pip_download_args.extend(["--requirement", str(req)])

    # Download packages
    _process = subprocess.run(
        pip_download_args,
        check=True,
    )

    return 0


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
