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
"""Generate a Python wheel for a pw_python_package or pw_python_distribution."""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys

try:
    from pw_build.python_package import PythonPackage

except ImportError:
    # Load from python_package from this directory if pw_build is not available.
    from python_package import PythonPackage  # type: ignore


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--package-dir',
        type=Path,
        required=True,
        help='Path to the root gn build dir.',
    )
    parser.add_argument(
        '--out-dir',
        type=Path,
        required=True,
        help='requirement file to generate',
    )
    parser.add_argument(
        '--generate-hashes',
        action='store_true',
        help='Generate sha256 sums for the requirements.txt file.',
    )
    return parser.parse_args()


def main(
    package_dir: Path,
    out_dir: Path,
    generate_hashes: bool,
) -> int:
    """Build a Python wheel."""

    # Delete existing wheels from the out dir, there may be stale versions.
    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Find the target package name and version.
    pkg = PythonPackage(
        sources=[],
        setup_sources=[package_dir / 'setup.cfg'],
        tests=[],
        inputs=[],
    )
    requirement_lines = f'{pkg.package_name}=={pkg.package_version}'

    # Build the wheel.
    subprocess.run(
        [
            sys.executable,
            "-m",
            "build",
            str(package_dir),
            "--wheel",
            "--no-isolation",
            "--outdir",
            str(out_dir),
        ],
        check=True,
    )

    if generate_hashes:
        # Cacluate the sha256
        for wheel_path in out_dir.glob('**/*.whl'):
            wheel_sha256 = hashlib.sha256()
            wheel_sha256.update(wheel_path.read_bytes())
            sha256 = wheel_sha256.hexdigest()
            requirement_lines += ' \\\n    '
            requirement_lines += f'--hash=sha256:{sha256}'
            break

    # Save a requirements file for this wheel and hash value.
    requirement_file = out_dir / 'requirements.txt'
    requirement_file.write_text(requirement_lines, encoding='utf-8')

    return 0


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
