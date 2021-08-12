# Copyright 2021 The Pigweed Authors
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
"""Build a Python Source tree."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
from typing import Iterable, List

import setuptools  # type: ignore

from pw_build.python_package import PythonPackage


def _parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree-destination-dir',
                        type=Path,
                        help='Path to output directory.')
    parser.add_argument('--include-tests',
                        action='store_true',
                        help='Include tests in the tests dir.')
    parser.add_argument(
        '--extra-files',
        nargs='+',
        help='Paths to extra files that should be included in the output dir.')
    parser.add_argument(
        '--input-list-files',
        nargs='+',
        type=Path,
        help='Paths to text files containing lists of Python package metadata '
        'json files.')

    return parser.parse_args()


# TODO(tonymd): Implement a way to merge all configs into one.
def merge_configs():
    pass


def load_packages(input_list_files: Iterable[Path]) -> List[PythonPackage]:
    """Load Python package metadata and configs."""

    packages = []
    for input_path in input_list_files:

        with input_path.open() as input_file:
            # Each line contains the path to a json file.
            for json_file in input_file.readlines():
                # Load the json as a dict.
                json_file_path = Path(json_file.strip()).resolve()
                with json_file_path.open() as json_fp:
                    json_dict = json.load(json_fp)

                packages.append(PythonPackage.from_dict(**json_dict))
    return packages


def build_python_tree(python_packages: Iterable[PythonPackage],
                      tree_destination_dir: Path,
                      include_tests: bool = False) -> None:
    """Install PythonPackages to a destination directory."""

    # Save the current out directory
    out_dir = Path.cwd()

    # Create the root destination directory.
    destination_path = tree_destination_dir.resolve()
    # Delete any existing files
    shutil.rmtree(destination_path, ignore_errors=True)
    destination_path.mkdir(exist_ok=True)

    # Define a temporary location to run setup.py build in.
    with tempfile.TemporaryDirectory() as build_base_name:
        build_base = Path(build_base_name)
        lib_dir_path = build_base / 'lib'

        for pkg in python_packages:
            # Create the temp install dir
            lib_dir_path.mkdir(parents=True, exist_ok=True)

            # cd to the location of setup.py
            setup_dir_path = out_dir / pkg.setup_dir
            os.chdir(setup_dir_path)
            # Run build with temp build-base location
            # Note: New files will be placed inside lib_dir_path
            setuptools.setup(script_args=[
                'build',
                '--force',
                '--build-base',
                str(build_base),
            ])

            new_pkg_dir = lib_dir_path / pkg.package_name

            # If tests should be included, copy them to the tests dir
            if include_tests and pkg.tests:
                test_dir_path = new_pkg_dir / 'tests'
                test_dir_path.mkdir(parents=True, exist_ok=True)

                for test_source_path in pkg.tests:
                    shutil.copy(out_dir / test_source_path, test_dir_path)

            # Move installed files from the temp build-base into
            # destination_path.
            for new_file in lib_dir_path.glob('*'):
                # Use str(Path) since shutil.move only accepts path-like objects
                # in Python 3.9 and up:
                #   https://docs.python.org/3/library/shutil.html#shutil.move
                shutil.move(str(new_file), str(destination_path))

            # Clean build base lib folder for next install
            shutil.rmtree(lib_dir_path, ignore_errors=True)

    # cd back to out directory
    os.chdir(out_dir)


def copy_extra_files(extra_file_strings: Iterable[str]) -> None:
    """Copy extra files to their destinations."""
    if not extra_file_strings:
        return

    for extra_file_string in extra_file_strings:
        # Convert 'source > destination' strings to Paths.
        input_output = re.split(r' *> *', extra_file_string)
        source_file = Path(input_output[0])
        dest_file = Path(input_output[1])

        if not source_file.exists():
            raise FileNotFoundError(f'extra_file "{source_file}" not found.\n'
                                    f'  Defined by: "{extra_file_string}"')

        # Copy files and make parent directories.
        dest_file.parent.mkdir(parents=True, exist_ok=True)
        # Raise an error if the destination file already exists.
        if dest_file.exists():
            raise FileExistsError(
                f'Copying "{source_file}" would overwrite "{dest_file}"')

        shutil.copy(source_file, dest_file)


def main():
    args = _parse_args()

    py_packages = load_packages(args.input_list_files)

    build_python_tree(python_packages=py_packages,
                      tree_destination_dir=args.tree_destination_dir,
                      include_tests=args.include_tests)
    copy_extra_files(args.extra_files)


if __name__ == '__main__':
    main()
