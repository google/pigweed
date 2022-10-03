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
"""Configure Python IDE support for Pigweed projects."""

from collections import defaultdict
import os
from pathlib import Path
import platform
from typing import Dict, NamedTuple

from pw_ide.exceptions import UnsupportedPlatformException
from pw_ide.symlinks import set_symlink

PYTHON_SYMLINK_NAME = 'python'
PYTHON_BIN_DIR_SYMLINK_NAME = 'python-bin'

PYTHON_VENV_PATH = Path(
    os.path.expandvars('$_PW_ACTUAL_ENVIRONMENT_ROOT')) / 'pigweed-venv'

PW_PROJECT_ROOT = Path(os.path.expandvars('$PW_PROJECT_ROOT'))


class PythonPaths(NamedTuple):
    """Holds the name of platform-specific Python env paths.

    The directory layout of Python virtual environments varies among
    platforms. This class holds the data needed to find the right paths
    for a specific platform.
    """
    # Name of the binaries directory
    bin_dir: str = 'bin'
    # Name of the interpreter executable
    interpreter: str = 'python3'


# When given a platform (e.g. the output of platform.system()), this dict gives
# the platform-specific virtualenv path names.
PYTHON_PATHS: Dict[str, PythonPaths] = defaultdict(PythonPaths)
PYTHON_PATHS['Windows'] = PythonPaths(bin_dir='Scripts',
                                      interpreter='pythonw.exe')


def get_python_venv_path(system: str = platform.system()) -> Path:
    """Return the path to the Python virtual environment interpreter."""
    if system == '':
        raise UnsupportedPlatformException()

    (bin_dir, interpreter) = PYTHON_PATHS[system]
    abs_path = PYTHON_VENV_PATH / bin_dir / interpreter
    return abs_path.relative_to(PW_PROJECT_ROOT)


def create_python_symlink(
    working_dir: Path, system: str = platform.system()) -> None:
    """Create symlinks to the Python virtual environment.

    The location of the virtual environment varies depending on platform and
    environment directory. This provides a stable reference for IDE features.
    """
    if system == '':
        raise UnsupportedPlatformException()

    (bin_dir, interpreter) = PYTHON_PATHS[system]

    python_venv_bin_path = PYTHON_VENV_PATH / bin_dir
    python_venv_interpreter_path = python_venv_bin_path / interpreter

    interpreter_symlink_path = working_dir / PYTHON_SYMLINK_NAME
    set_symlink(python_venv_interpreter_path, interpreter_symlink_path)

    bin_dir_symlink_path = working_dir / PYTHON_BIN_DIR_SYMLINK_NAME
    set_symlink(python_venv_bin_path, bin_dir_symlink_path)
