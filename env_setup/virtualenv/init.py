# Copyright 2019 The Pigweed Authors
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
"""Sets up a Python 3 virtualenv for Pigweed."""

import argparse
import subprocess
import venv
import os
import shlex
import sys
from typing import Sequence


def git_stdout(*args, **kwargs) -> str:
    return subprocess.run(('git', *args),
                          stdout=subprocess.PIPE,
                          check=True,
                          **kwargs).stdout.decode().strip()


def git_list_files(*args, **kwargs) -> Sequence[str]:
    return git_stdout('ls-files', *args, **kwargs).split()


def git_repo_root(path: str = './') -> str:
    return git_stdout('-C', path, 'rev-parse', '--show-toplevel')


def init(venv_path, requirements=()) -> None:
    """Creates a venv and installs all packages in this Git repo."""

    print('Creating venv at', venv_path)
    venv.create(venv_path, clear=True, with_pip=True)

    venv_python = os.path.join(venv_path, 'bin', 'python')

    def pip_install(*args):
        cmd = venv_python, '-m', 'pip', 'install', *args
        print(shlex.join(cmd))
        return subprocess.run(cmd, check=True)

    pip_install('--upgrade', 'pip')

    package_args = tuple(
        f'--editable={os.path.dirname(path)}' for path in git_list_files(
            'setup.py', '*/setup.py', cwd=git_repo_root()))

    requirement_args = tuple(f'--requirement={req}' for req in requirements)

    pip_install('--log', os.path.join(venv_path, 'pip.log'), *requirement_args,
                *package_args)


def _main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--venv_path',
                        required=True,
                        help='Path at which to create the venv')
    parser.add_argument('-r',
                        '--requirements',
                        default=[],
                        action='append',
                        help='requirements.txt files to install')

    init(**vars(parser.parse_args()))


if __name__ == '__main__':
    _main()
