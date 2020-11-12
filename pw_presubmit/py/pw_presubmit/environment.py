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
"""Functions for initializing CIPD and the Pigweed virtualenv."""

import logging
import os
from pathlib import Path
import sys
from typing import Iterable, Union

from pw_presubmit import call

_LOG = logging.getLogger(__name__)


def init_cipd(
    pigweed_root: Path,
    output_directory: Path,
    package_files: Iterable[Path] = ()) -> None:
    """Runs CIPD."""

    # TODO(mohrr): invoke by importing rather than by subprocess.

    cmd = [
        sys.executable,
        pigweed_root.joinpath('pw_env_setup', 'py', 'pw_env_setup',
                              'cipd_setup', 'update.py'),
        '--install-dir', output_directory,
    ]  # yapf: disable

    final_package_files = list(
        pigweed_root.joinpath('pw_env_setup', 'py', 'pw_env_setup',
                              'cipd_setup').glob('*.json'))
    final_package_files.extend(package_files)

    for package_file in final_package_files:
        cmd.extend(('--package-file', package_file))

    call(*cmd)

    paths = [output_directory, output_directory.joinpath('bin')]
    for base in output_directory.glob('*'):
        paths.append(base)
        paths.append(base.joinpath('bin'))

    paths.append(Path(os.environ['PATH']))

    os.environ['PATH'] = os.pathsep.join(str(x) for x in paths)
    _LOG.debug('PATH %s', os.environ['PATH'])


def init_virtualenv(
        pigweed_root: Path,
        output_directory: Path,
        requirements: Iterable[Union[Path, str]] = (),
        gn_targets: Iterable[str] = (),
) -> None:
    """Sets up a virtualenv, assumes recent Python 3 is already installed."""
    virtualenv_source = pigweed_root.joinpath('pw_env_setup', 'py',
                                              'pw_env_setup',
                                              'virtualenv_setup')

    # Need to set VIRTUAL_ENV before invoking GN because the GN targets install
    # directly to the current virtual env.
    os.environ['VIRTUAL_ENV'] = str(output_directory)
    os.environ['PATH'] = os.pathsep.join((
        str(output_directory.joinpath('bin')),
        os.environ['PATH'],
    ))

    if not gn_targets:
        gn_targets = (f'{os.environ["PW_ROOT"]}#:python.install', )

    call(
        'python3',
        virtualenv_source,
        f'--venv_path={output_directory}',
        f'--requirements={virtualenv_source / "requirements.txt"}',
        *(f'--requirements={x}' for x in requirements),
        *(f'--gn-target={t}' for t in gn_targets),
    )
