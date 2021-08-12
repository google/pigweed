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
import copy
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


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
