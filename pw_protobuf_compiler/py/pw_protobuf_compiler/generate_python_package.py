# Copyright 2020 The Pigweed Authors
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
"""Generates a setup.py and __init__.py for a Python package."""

import argparse
from pathlib import Path
import sys
from typing import List

# Make sure dependencies are optional, since this script may be run when
# installing Python package dependencies through GN.
try:
    from pw_cli.log import install as setup_logging
except ImportError:
    from logging import basicConfig as setup_logging  # type: ignore

_SETUP_TEMPLATE = """# Generated file. Do not modify.
import setuptools

setuptools.setup(
    name='<PACKAGE_NAME>',
    version='0.0.1',
    author='Pigweed Authors',
    author_email='pigweed-developers@googlegroups.com',
    description='Generated protobuf files',
    packages=setuptools.find_packages(),
    zip_safe=False,
)
"""


def _parse_args():
    """Parses and returns the command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',
                        required=True,
                        help='Name of the generated Python package')
    parser.add_argument('--setup',
                        required=True,
                        type=Path,
                        help='Path to setup.py file')
    parser.add_argument('subpackages',
                        type=Path,
                        nargs='+',
                        help='Subpackage paths within the package')

    return parser.parse_args()


def main(package: str, setup: Path, subpackages: List[Path]) -> int:
    setup.parent.mkdir(exist_ok=True)

    for subpackage in set(subpackages):
        package_dir = setup.parent / subpackage
        package_dir.mkdir(exist_ok=True, parents=True)
        package_dir.joinpath('__init__.py').touch()

    setup.write_text(_SETUP_TEMPLATE.replace('<PACKAGE_NAME>', package))
    return 0


if __name__ == '__main__':
    setup_logging()
    sys.exit(main(**vars(_parse_args())))
