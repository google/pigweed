#!/usr/bin/env python
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
"""Installs or updates prebuilt tools.

Must be tested with Python 2 and Python 3.

The stdout of this script is meant to be executed by the invoking shell.
"""

from __future__ import print_function

import argparse
import glob
import os
import subprocess
import sys
import tempfile

SCRIPT_ROOT = os.path.abspath(os.path.dirname(__file__))
GIT_ROOT = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'],
                                   cwd=SCRIPT_ROOT).decode('utf-8').strip()


def parse(argv=None):
    """Parse arguments."""

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        '--install-dir',
        dest='root_install_dir',
        default=os.path.join(GIT_ROOT, '.cipd'),
    )
    parser.add_argument('--ensure-file', dest='ensure_files', action='append')
    parser.add_argument('--cipd', default=os.path.join(SCRIPT_ROOT, 'cipd.py'))
    parser.add_argument('--suppress-shell-commands',
                        action='store_false',
                        dest='print_shell_commands')
    parser.add_argument('--cache-dir',
                        default=os.environ.get(
                            'CIPD_CACHE_DIR',
                            os.path.expanduser('~/.cipd-cache-dir')))

    return parser.parse_args(argv)


def check_auth(cipd, print_shell_commands):
    """Check logged into CIPD."""
    try:
        subprocess.check_output([cipd, 'auth-info'], stderr=subprocess.STDOUT)
        return True

    except subprocess.CalledProcessError:
        print('=' * 60, file=sys.stderr)
        print('ERROR: not logged into CIPD--please run this command:',
              file=sys.stderr)
        print(cipd, 'auth-login', file=sys.stderr)
        print('=' * 60, file=sys.stderr)

        if print_shell_commands:
            with tempfile.NamedTemporaryFile(mode='w',
                                             delete=False,
                                             prefix='cipdsetup') as temp:
                print('ABORT_PW_ENVSETUP=1', file=temp)

            print('. {}'.format(temp.name))
        return False


def update(cipd, ensure_files, root_install_dir, cache_dir,
           print_shell_commands):
    """Grab the tools listed in ensure_file."""

    os.environ['CIPD_PY_INSTALL_DIR'] = root_install_dir
    os.environ['CIPD_CACHE_DIR'] = cache_dir

    if not check_auth(cipd, print_shell_commands):
        return

    if not os.path.isdir(root_install_dir):
        os.makedirs(root_install_dir)

    paths = [root_install_dir]
    env = {
        'CIPD_INSTALL_DIR': root_install_dir,
        'CIPD_CACHE_DIR': cache_dir,
    }

    default_ensures = os.path.join(SCRIPT_ROOT, '*.ensure')
    for ensure_file in ensure_files or glob.glob(default_ensures):
        install_dir = os.path.join(root_install_dir,
                                   os.path.basename(ensure_file))

        cmd = [
            cipd,
            'ensure',
            '-ensure-file', ensure_file,
            '-root', install_dir,
            '-log-level', 'warning',
            '-max-threads', '0',  # 0 means use CPU count.
        ]  # yapf: disable

        print(*cmd, file=sys.stderr)
        subprocess.check_call(cmd, stdout=sys.stderr)

        paths.append(install_dir)
        paths.append(os.path.join(install_dir, 'bin'))

        name = ensure_file
        if os.path.splitext(name)[1] == '.ensure':
            name = os.path.splitext(name)[0]
        name = os.path.basename(name)
        env['{}_CIPD_INSTALL_DIR'.format(name.upper())] = install_dir

    for path in paths:
        print('adding {} to path'.format(path), file=sys.stderr)

    paths.append('$PATH')

    if print_shell_commands:
        with tempfile.NamedTemporaryFile(mode='w',
                                         delete=False,
                                         prefix='cipdsetup') as temp:
            print('PATH="{}"'.format(os.pathsep.join(paths)), file=temp)
            print('export PATH', file=temp)
            for name, value in env.items():
                print('{}={}'.format(name, value), file=temp)
                print('export {}'.format(name), file=temp)

            print('. {}'.format(temp.name))


if __name__ == '__main__':
    update(**vars(parse()))
    sys.exit(0)
