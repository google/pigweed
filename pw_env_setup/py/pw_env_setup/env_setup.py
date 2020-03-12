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
            frame = inspect.currentframe()
            if frame is not None:
                filename = inspect.getfile(frame)
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
from pw_env_setup.colors import Color, enable_colors
from pw_env_setup import cargo_setup
from pw_env_setup import environment
from pw_env_setup import virtualenv_setup


class _Result:
    class Status:  # pylint: disable=too-few-public-methods
        DONE = 'done'
        SKIPPED = 'skipped'

    def __init__(self, status, *messages):
        self._status = status
        self._messages = list(messages)

    def status_str(self):
        return self._status

    def messages(self):
        return self._messages


# TODO(mohrr) remove disable=useless-object-inheritance once in Python 3.
# pylint: disable=useless-object-inheritance
class EnvSetup(object):
    """Run environment setup for Pigweed."""
    def __init__(self, pw_root, cipd_cache_dir, shell_file, quiet, *args,
                 **kwargs):
        super(EnvSetup, self).__init__(*args, **kwargs)
        self._env = environment.Environment()
        self._pw_root = pw_root
        self._setup_root = os.path.join(pw_root, 'pw_env_setup', 'py',
                                        'pw_env_setup')
        self._cipd_cache_dir = cipd_cache_dir
        self._shell_file = shell_file
        self._is_windows = os.name == 'nt'
        self._quiet = quiet

        if os.path.isfile(shell_file):
            os.unlink(shell_file)

        if isinstance(self._pw_root, bytes) and bytes != str:
            self._pw_root = self._pw_root.decode()

        self._env.set('PW_ROOT', self._pw_root)

    def _log(self, *args, **kwargs):
        # Not using logging module because it's awkward to flush a log handler.
        if self._quiet:
            return
        flush = kwargs.pop('flush', False)
        print(*args, **kwargs)
        if flush:
            sys.stdout.flush()

    def setup(self):
        """Runs each of the env_setup steps."""

        enable_colors()

        steps = [
            ('CIPD package manager', self.cipd),
            ('Python environment', self.virtualenv),
            ('Pigweed host tools', self.host_tools),
        ]

        # TODO(pwbug/67): Rust isn't currently used anywhere, so this is
        # commented out to avoid cluttering the bootstrap output. It should be
        # re-enabled once we have a use for Rust.
        #
        # TODO(pwbug/63): Add a Windows version of cargo to CIPD.
        #
        # if not self._is_windows:
        #   steps.append(("Rust's cargo", self.cargo))

        self._log(
            Color.bold('Downloading and installing packages into local '
                       'source directory:\n'))

        max_name_len = max(len(name) for name, _ in steps)

        self._env.echo(
            Color.bold(
                'Activating environment (setting environment variables):'))
        self._env.echo('')

        for name, step in steps:
            self._log('  Setting up {name:.<{width}}...'.format(
                name=name, width=max_name_len),
                      end='',
                      flush=True)
            self._env.echo(
                '  Setting environment variables for {name:.<{width}}...'.
                format(name=name, width=max_name_len),
                newline=False,
            )

            result = step()

            self._env.echo(result.status_str())
            for message in result.messages():
                self._env.echo(message)

            self._log('done')

        self._log('')
        self._env.echo('')

        with open(self._shell_file, 'w') as outs:
            self.write_preamble(outs)
            self._env.write(outs)
            self.write_sanity_check(outs)

    def cipd(self):
        install_dir = os.path.join(self._pw_root, '.cipd')

        cipd_client = cipd_wrapper.init(install_dir, silent=True)

        package_files = glob.glob(
            os.path.join(self._setup_root, 'cipd_setup', '*.json'))
        cipd_update.update(
            cipd=cipd_client,
            root_install_dir=install_dir,
            package_files=package_files,
            cache_dir=self._cipd_cache_dir,
            env_vars=self._env,
        )

        return _Result(_Result.Status.DONE)

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
        if self._is_windows:
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

        virtualenv_setup.install(
            venv_path=venv_path,
            requirements=[requirements],
            python=python,
            env=self._env,
        )

        return _Result(_Result.Status.DONE)

    def host_tools(self):
        # The host tools are grabbed from CIPD, at least initially. If the
        # user has a current host build, that build will be used instead.
        host_dir = os.path.join(self._pw_root, 'out', 'host')
        self._env.prepend('PATH', os.path.join(host_dir, 'host_tools'))
        return _Result(_Result.Status.DONE)

    def cargo(self):
        if not os.environ.get('PW_CARGO_SETUP', ''):
            return _Result(
                _Result.Status.SKIPPED,
                '    Note: Re-run bootstrap with PW_CARGO_SETUP=1 set '
                'in your environment',
                '          to enable Rust.',
            )

        cargo_setup.install(pw_root=self._pw_root, env=self._env)
        return _Result(_Result.Status.DONE)

    def write_preamble(self, fd):
        def comment(*args, **kwargs):
            kwargs['file'] = fd
            comment_char = '::' if self._is_windows else '#'
            print(comment_char, *args, **kwargs)

        comment('This file is automatically generated. DO NOT EDIT!')
        comment('For details, see '
                '$PW_ROOT/pw_env_setup/py/pw_env_setup/env_setup.py and ')
        comment('$PW_ROOT/pw_env_setup/py/pw_env_setup/environment.py.')
        if not self._is_windows:
            comment('For help debugging errors in this script, uncomment the '
                    'next line.')
            comment('set -x')
            comment('Then use `set +x` to go back to normal.')
        fd.write('\n')

    def write_sanity_check(self, fd):
        """Call pw doctor after setting environment variables."""

        echo_empty = 'echo.' if self._is_windows else 'echo'

        if not self._quiet:
            # Not quoting args to echo because Windows will treat them as if
            # they're already quoted and Linux will just space-separate them.
            fd.write('echo {}\n'.format(
                Color.bold('Sanity checking the environment:')))
            fd.write('{}\n'.format(echo_empty))

        log_level = 'warn' if 'PW_ENVSETUP_QUIET' in os.environ else 'info'
        doctor = ' '.join(
            ['pw', '--no-banner', '--loglevel', log_level, 'doctor'])

        if self._is_windows:
            fd.write('{}\n'.format(doctor))
            fd.write('if %ERRORLEVEL% EQU 0 (\n')
        else:
            fd.write('if {}; then\n'.format(doctor))

        if not self._quiet:
            fd.write('  {}\n'.format(echo_empty))
            # Again, not quoting args to echo.
            fd.write('  echo {}\n'.format(
                Color.bold('Environment looks good, you are ready to go!')))

        if self._is_windows:
            fd.write(')\n')
        else:
            # If PW_ENVSETUP_QUIET is set, there might not be anything inside
            # the if which is an error. Always echo nothing at the end.
            fd.write('  echo -n\n')
            fd.write('fi\n')


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

    parser.add_argument(
        '--quiet',
        help='Reduce output.',
        action='store_true',
        default='PW_ENVSETUP_QUIET' in os.environ,
    )

    return parser.parse_args(argv)


def main():
    return EnvSetup(**vars(parse())).setup()


if __name__ == '__main__':
    sys.exit(main())
