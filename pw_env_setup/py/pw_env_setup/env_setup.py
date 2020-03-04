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
import copy
import glob
import inspect
import os
import shutil
import subprocess
import sys

# TODO(pwbug/67): Remove import hacks once the oxidized prebuilt binaries are
# proven stable for first-time bootstrapping. For now, continue to support
# running directly from source without assuming a functioning Python
# environment when running for the first time.

# If we're running oxidized, filesystem-centric import hacks won't work. In that
# case, jump straight to the imports and assume oxidation brought in the deps.
if not getattr(sys, 'oxidized', False):
    try:
        # Even if we're running from source, the user may have a functioning
        # Python environment already set up. Prefer using it over hacks.
        # pylint: disable=no-name-in-module
        from pw_env_setup import cargo_setup
        # pylint: enable=no-name-in-module
    except ImportError:
        old_sys_path = copy.deepcopy(sys.path)
        filename = None
        if hasattr(sys.modules[__name__], '__file__'):
            filename = __file__
        else:
            # Try introspection in environments where __file__ is not populated.
            filename = inspect.getfile(inspect.currentframe())
        # If none of our strategies worked, the imports are going to fail.
        if filename is None:
            raise
        sys.path.append(
            os.path.abspath(
                os.path.join(filename, os.path.pardir, os.path.pardir)))
        import pw_env_setup  # pylint: disable=unused-import
        sys.path = old_sys_path

# pylint: disable=wrong-import-position
from pw_env_setup.cipd_setup import update as cipd_update
from pw_env_setup.cipd_setup import wrapper as cipd_wrapper
from pw_env_setup import cargo_setup
from pw_env_setup import environment
from pw_env_setup import virtualenv_setup


# TODO(mohrr) remove disable=useless-object-inheritance once in Python 3.
# pylint: disable=useless-object-inheritance
class EnvSetup(object):
    """Run environment setup for Pigweed."""
    def __init__(self, pw_root, cipd_cache_dir, shell_file, *args, **kwargs):
        super(EnvSetup, self).__init__(*args, **kwargs)
        self._env = environment.Environment()
        self._pw_root = pw_root
        self._setup_root = os.path.join(pw_root, 'pw_env_setup', 'py',
                                        'pw_env_setup')
        self._cipd_cache_dir = cipd_cache_dir
        self._shell_file = shell_file

        if os.path.isfile(shell_file):
            os.unlink(shell_file)

        if isinstance(self._pw_root, bytes) and bytes != str:
            self._pw_root = self._pw_root.decode()

        self._env.set('PW_ROOT', self._pw_root)

    def setup(self):
        steps = [
            ('cipd', self.cipd),
            ('python', self.virtualenv),
            ('host_tools', self.host_tools),
        ]

        if os.name != 'nt':
            # TODO(pwbug/63): Add a Windows version of cargo to CIPD.
            steps.append(('cargo', self.cargo))

        for name, step in steps:
            print('Setting up {}...'.format(name), file=sys.stderr)
            step()
            print('  done.', file=sys.stderr)

        with open(self._shell_file, 'w') as outs:
            self._env.write(outs)
            if 'PW_ENVSETUP_QUIET' in os.environ:
                outs.write('pw --loglevel warn doctor\n')
            else:
                outs.write('pw --loglevel info doctor\n')
            outs.write('echo Pigweed environment setup complete')

    def cipd(self):
        install_dir = os.path.join(self._pw_root, '.cipd')

        cipd_client = cipd_wrapper.init(install_dir)

        package_files = glob.glob(
            os.path.join(self._setup_root, 'cipd_setup', '*.json'))
        self._env.echo('Setting CIPD environment variables...')
        cipd_update.update(
            cipd=cipd_client,
            root_install_dir=install_dir,
            package_files=package_files,
            cache_dir=self._cipd_cache_dir,
            env_vars=self._env,
        )
        self._env.echo('  done.')

    def virtualenv(self):
        """Setup virtualenv."""

        venv_path = os.path.join(self._pw_root, '.python3-env')

        requirements = os.path.join(self._setup_root, 'virtualenv_setup',
                                    'requirements.txt')

        cipd_bin = os.path.join(
            self._pw_root,
            '.cipd',
            'pigweed',
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

        self._env.echo('Setting virtualenv environment variables...')
        virtualenv_setup.install(
            venv_path=venv_path,
            requirements=[requirements],
            python=python,
            env=self._env,
        )
        self._env.echo('  done.')

    def host_tools(self):
        # The host tools are grabbed from CIPD, at least initially. If the
        # user has a current host build, that build will be used instead.
        self._env.echo('Setting host_tools environment variables...')
        host_dir = os.path.join(self._pw_root, 'out', 'host')
        self._env.prepend('PATH', os.path.join(host_dir, 'host_tools'))
        self._env.echo('  done.')

    def cargo(self):
        self._env.echo('Setting cargo environment variables...')
        if os.environ.get('PW_CARGO_SETUP', ''):
            cargo_setup.install(pw_root=self._pw_root, env=self._env)
        else:
            self._env.echo(
                '  cargo setup skipped, set PW_CARGO_SETUP to include it')
        self._env.echo('  done.')


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
