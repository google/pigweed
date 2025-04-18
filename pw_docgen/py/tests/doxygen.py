# Copyright 2025 The Pigweed Authors
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
"""Run Doxygen for the GN docs build tests."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def _parse_args() -> argparse.Namespace:
    """Parse CLI args that are passed when the script is invoked from GN."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--outdir",
        type=Path,
        required=True,
        help="Directory where Doxygen output should be written.",
    )
    parser.add_argument(
        "--doxyfile",
        type=Path,
        required=True,
        help="Path to the Doxygen configuration file.",
    )
    parser.add_argument(
        "--data",
        type=Path,
        required=True,
        help="Path to the header file that's used as Doxygen input.",
    )
    return parser.parse_args()


def main(
    outdir: Path,
    doxyfile: Path,
    data: Path,
) -> None:
    """Invoke Doxygen with data provided from the GN build."""
    env = os.environ.copy()
    env["GN_DOCS_BUILD_TESTS_OUTPUT_DIRECTORY"] = str(outdir.resolve())
    env["GN_DOCS_BUILD_TESTS_INPUT"] = str(data.resolve())
    shutil.rmtree(outdir, ignore_errors=True)
    outdir.mkdir(parents=True, exist_ok=True)
    command = ["doxygen", str(doxyfile.resolve())]
    process = subprocess.run(command, env=env, check=True)
    sys.exit(process.returncode)


if __name__ == "__main__":
    main(**vars(_parse_args()))
