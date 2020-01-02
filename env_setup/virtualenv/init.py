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
import glob
import os
import shlex
import subprocess
import sys
import venv
from typing import Sequence


def git_stdout(*args, **kwargs) -> str:
    return subprocess.run(('git', *args),
                          stdout=subprocess.PIPE,
                          check=True,
                          **kwargs).stdout.decode().strip()


def git_list_files(*args, **kwargs) -> Sequence[str]:
    return git_stdout('ls-files', *args, **kwargs).split()


def git_repo_root(path: str = './') -> str:
    try:
        return git_stdout('-C', path, 'rev-parse', '--show-toplevel')
    except subprocess.CalledProcessError:
        return None


class GitRepoNotFound(Exception):
    pass


def _installed_packages(venv_python):
    cmd = (venv_python, '-m', 'pip', 'list', '--disable-pip-version-check')
    output = subprocess.run(cmd, stdout=subprocess.PIPE,
                            check=True).stdout.decode().splitlines()
    return set(x.split()[0].lower() for x in output[2:])


def _required_packages(requirements):
    packages = []

    for req in requirements:
        with open(req, 'r') as ins:
            for line in ins:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                packages.append(line.split('=')[0])

    return set(packages)


def _pw_package_names(setup_py_files):
    # TODO(pwbug/42) find a more reliable way to get this.
    del setup_py_files  # Unused for now.
    return set((
        'pw-bloat',
        'pw-cli',
        'pw-module',
        'pw-presubmit',
        'pw-unit-test',
        'stm32f429i-disc1-utils',
    ))


def init(venv_path, full_envsetup: bool = True, requirements=()) -> None:
    """Creates a venv and installs all packages in this Git repo."""

    pyvenv_cfg = os.path.join(venv_path, 'pyvenv.cfg')
    if full_envsetup or not os.path.exists(pyvenv_cfg):
        print('Creating venv at', venv_path)
        venv.create(venv_path, clear=True, with_pip=True)

    venv_python = os.path.join(venv_path, 'bin', 'python')

    pw_root = os.environ.get('PW_ROOT', git_repo_root())
    if not pw_root:
        raise GitRepoNotFound()

    setup_py_files = git_list_files('setup.py', '*/setup.py', cwd=pw_root)

    # If not forcing full setup, check if all expected packages are installed,
    # ignoring versions. If they are, skip reinstalling.
    if not full_envsetup:
        installed = _installed_packages(venv_python)
        required = _required_packages(requirements)
        pw_pkgs = _pw_package_names(setup_py_files)

        if required.issubset(installed) and pw_pkgs.issubset(installed):
            print('Python packages already installed, exiting')
            return

        # Sometimes we get an error saying "Egg-link ... does not match
        # installed location". This gets around that. The egg-link files
        # all come from 'pw'-prefixed packages we installed with --editable.
        # Source: https://stackoverflow.com/a/48972085
        for egg_link in glob.glob(
                os.path.join(venv_path,
                             'lib/python*/site-packages/*.egg-link')):
            os.unlink(egg_link)

    def pip_install(*args):
        cmd = venv_python, '-m', 'pip', 'install', *args
        print(shlex.join(cmd))
        return subprocess.run(cmd, check=True)

    pip_install('--upgrade', 'pip')

    package_args = tuple(
        f'--editable={os.path.join(pw_root, os.path.dirname(path))}'
        for path in setup_py_files)

    requirement_args = tuple(f'--requirement={req}' for req in requirements)

    pip_install('--log', os.path.join(venv_path, 'pip.log'), *requirement_args,
                *package_args)


def _main() -> None:
    expected_version = (3, 8)
    if sys.version_info[0:2] != expected_version:
        print('Bad Python version detected: {}'.format(sys.version),
              file=sys.stderr)
        print('Expected: {}'.format('.'.join(str(x)
                                             for x in expected_version)),
              file=sys.stderr)
        return -1

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--venv_path',
                        required=True,
                        help='Path at which to create the venv')
    parser.add_argument('-r',
                        '--requirements',
                        default=[],
                        action='append',
                        help='requirements.txt files to install')
    parser.add_argument('--quick-setup',
                        dest='full_envsetup',
                        action='store_false',
                        default='PW_ENVSETUP_FULL' in os.environ,
                        help=('Do full setup or only minimal checks to see if '
                              'full setup is required.'))

    try:
        init(**vars(parser.parse_args()))
    except GitRepoNotFound:
        print('git repository not found', file=sys.stderr)
        return -1

    return 0


if __name__ == '__main__':
    sys.exit(_main())
