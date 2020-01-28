#!/usr/bin/env python

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
"""Environment setup script for Pigweed.

This script installs everything and writes out a file for the user's shell
to source.

For now, this is valid Python 2 and Python 3. Once we switch to running this
with PyOxidizer it can be upgraded to recent Python 3.
"""

from __future__ import print_function

import argparse
import glob
import os
import shutil
import subprocess
import sys

# TODO(mohrr) remove import-error disabling, not sure why pylint has issues
# with it.
import cipd.update  # pylint: disable=import-error
import cipd.wrapper  # pylint: disable=import-error
import environment  # pylint: disable=import-error
import host_build.init  # pylint: disable=import-error
import cargo.init  # pylint: disable=import-error
import virtualenv.init  # pylint: disable=import-error


# TODO(mohrr) remove disable=useless-object-inheritance once in Python 3.
# pylint: disable=useless-object-inheritance
class EnvSetup(object):
    """Run environment setup for Pigweed."""
    def __init__(self, pw_root, cipd_cache_dir, shell_file, *args, **kwargs):
        super(EnvSetup, self).__init__(*args, **kwargs)
        self._env = environment.Environment()
        self._pw_root = pw_root
        self._cipd_cache_dir = cipd_cache_dir
        self._shell_file = shell_file

        if os.path.isfile(shell_file):
            os.unlink(shell_file)

        if isinstance(self._pw_root, bytes):
            self._pw_root = self._pw_root.decode()

        self._env.set('PW_ROOT', self._pw_root)

    def setup(self):
        steps = [
            ('cipd', self.cipd),
            ('python', self.virtualenv),
            ('host_tools', self.host_build),
        ]

        if os.name != 'nt':
            # TODO(pwbug/63): Add a Windows version of cargo to CIPD.
            steps.append(('cargo', self.cargo))

        for name, step in steps:
            print('Setting up {}...\n'.format(name), file=sys.stdout)
            step()
            print('\nSetting up {}...done.'.format(name), file=sys.stdout)

        with open(self._shell_file, 'w') as outs:
            self._env.write(outs)

    def cipd(self):
        install_dir = os.path.join(self._pw_root, '.cipd')

        cipd_client = cipd.wrapper.init(install_dir)

        ensure_files = glob.glob(
            os.path.join(self._pw_root, 'env_setup', 'cipd', '*.ensure'))
        cipd.update.update(
            cipd=cipd_client,
            root_install_dir=install_dir,
            ensure_files=ensure_files,
            cache_dir=self._cipd_cache_dir,
            env_vars=self._env,
        )

    def virtualenv(self):
        venv_path = os.path.join(self._pw_root, '.python3-env')

        requirements = os.path.join(self._pw_root, 'env_setup', 'virtualenv',
                                    'requirements.txt')

        cipd_bin = os.path.join(
            self._pw_root,
            '.cipd',
            'pigweed.ensure',
            'bin',
        )
        if os.name == 'nt':
            # There is an issue with the virtualenv module on Windows where it
            # expects sys.executable to be called "python.exe" or it fails to
            # properly execute. Create a copy of python3.exe called python.exe
            # so that virtualenv works.
            old_python = os.path.join(cipd_bin, 'python3.exe')
            new_python = os.path.join(cipd_bin, 'python.exe')
            if not os.path.exists(new_python):
                shutil.copyfile(old_python, new_python)
            py_executable = 'python.exe'
        else:
            py_executable = 'python3'

        python = os.path.join(cipd_bin, py_executable)

        virtualenv.init.init(
            venv_path=venv_path,
            requirements=[requirements],
            python=python,
            env=self._env,
        )

    def host_build(self):
        host_build.init.init(pw_root=self._pw_root, env=self._env)

    def cargo(self):
        cargo.init.init(pw_root=self._pw_root, env=self._env)


def parse(argv=None):
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()

    pw_root = os.environ.get('PW_ROOT', None)
    if not pw_root:
        try:
            with open(os.devnull, 'w') as outs:
                pw_root = subprocess.check_output(
                    ['git', 'rev-parse', '--show-toplevel'],
                    stderr=outs).strip()
        except subprocess.CalledProcessError:
            pw_root = None
    parser.add_argument(
        '--pw-root',
        default=pw_root,
        required=not pw_root,
    )

    parser.add_argument(
        '--cipd-cache-dir',
        default=os.environ.get('CIPD_CACHE_DIR',
                               os.path.expanduser('~/.cipd-cache-dir')),
    )

    parser.add_argument(
        '--shell-file',
        help='Where to write the file for shells to source.',
        required=True,
    )

    return parser.parse_args(argv)


def main():
    return EnvSetup(**vars(parse())).setup()


if __name__ == '__main__':
    sys.exit(main())
