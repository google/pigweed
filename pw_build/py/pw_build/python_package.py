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
"""Dataclass for a Python package."""

import configparser
from contextlib import contextmanager
import copy
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
from typing import Dict, List, Optional, Iterable

import setuptools  # type: ignore


@contextmanager
def change_working_dir(directory: Path):
    original_dir = Path.cwd()
    try:
        os.chdir(directory)
        yield directory
    finally:
        os.chdir(original_dir)


@dataclass
class PythonPackage:
    """Class to hold a single Python package's metadata."""

    sources: List[Path]
    setup_sources: List[Path]
    tests: List[Path]
    inputs: List[Path]
    gn_target_name: Optional[str] = None
    generate_setup: Optional[Dict] = None
    config: Optional[configparser.ConfigParser] = None

    @staticmethod
    def from_dict(**kwargs) -> 'PythonPackage':
        """Build a PythonPackage instance from a dictionary."""
        transformed_kwargs = copy.copy(kwargs)

        # Transform string filenames to Paths
        for attribute in ['sources', 'tests', 'inputs', 'setup_sources']:
            transformed_kwargs[attribute] = [
                Path(s) for s in kwargs[attribute]
            ]

        return PythonPackage(**transformed_kwargs)

    def __post_init__(self):
        # Read the setup.cfg file if possible
        if not self.config:
            self.config = self._load_config()

    @property
    def setup_dir(self) -> Path:
        assert len(self.setup_sources) > 0
        # Assuming all setup_source files live in the same parent directory.
        return self.setup_sources[0].parent

    @property
    def setup_py(self) -> Path:
        setup_py = [
            setup_file for setup_file in self.setup_sources
            if str(setup_file).endswith('setup.py')
        ]
        # setup.py will not exist for GN generated Python packages
        assert len(setup_py) == 1
        return setup_py[0]

    @property
    def setup_cfg(self) -> Path:
        setup_cfg = [
            setup_file for setup_file in self.setup_sources
            if str(setup_file).endswith('setup.cfg')
        ]
        assert len(setup_cfg) == 1
        return setup_cfg[0]

    @property
    def package_name(self) -> str:
        assert self.config
        return self.config['metadata']['name']

    @property
    def package_dir(self) -> Path:
        return self.setup_cfg.parent / self.package_name

    def _load_config(self) -> Optional[configparser.ConfigParser]:
        config = configparser.ConfigParser()
        # Check for a setup.cfg and load that config.
        if self.setup_cfg:
            with self.setup_cfg.open() as config_file:
                config.read_file(config_file)
            return config
        return None

    def setuptools_build_with_base(self,
                                   build_base: Path,
                                   include_tests: bool = False) -> Path:
        # Create the lib install dir in case it doesn't exist.
        lib_dir_path = build_base / 'lib'
        lib_dir_path.mkdir(parents=True, exist_ok=True)

        starting_directory = Path.cwd()
        # cd to the location of setup.py
        with change_working_dir(self.setup_dir):
            # Run build with temp build-base location
            # Note: New files will be placed inside lib_dir_path
            setuptools.setup(script_args=[
                'build',
                '--force',
                '--build-base',
                str(build_base),
            ])

            new_pkg_dir = lib_dir_path / self.package_name
            # If tests should be included, copy them to the tests dir
            if include_tests and self.tests:
                test_dir_path = new_pkg_dir / 'tests'
                test_dir_path.mkdir(parents=True, exist_ok=True)

                for test_source_path in self.tests:
                    shutil.copy(starting_directory / test_source_path,
                                test_dir_path)

        return lib_dir_path

    def setuptools_develop(self) -> None:
        with change_working_dir(self.setup_dir):
            setuptools.setup(script_args=['develop'])

    def setuptools_install(self) -> None:
        with change_working_dir(self.setup_dir):
            setuptools.setup(script_args=['install'])


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
