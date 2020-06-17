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
from pw_env_setup import spinner
from pw_env_setup import virtualenv_setup


# TODO(pwbug/67, pwbug/68) switch to shutil.which().
def _which(executable,
           pathsep=os.pathsep,
           use_pathext=None,
           case_sensitive=None):
    if use_pathext is None:
        use_pathext = (os.name == 'nt')
    if case_sensitive is None:
        case_sensitive = (os.name != 'nt' and sys.platform != 'darwin')

    if not case_sensitive:
        executable = executable.lower()

    exts = None
    if use_pathext:
        exts = frozenset(os.environ['PATHEXT'].split(pathsep))
        if not case_sensitive:
            exts = frozenset(x.lower() for x in exts)
        if not exts:
            raise ValueError('empty PATHEXT')

    paths = os.environ['PATH'].split(pathsep)
    for path in paths:
        try:
            entries = frozenset(os.listdir(path))
            if not case_sensitive:
                entries = frozenset(x.lower() for x in entries)
        except OSError:
            continue

        if exts:
            for ext in exts:
                if executable + ext in entries:
                    return os.path.join(path, executable + ext)
        else:
            if executable in entries:
                return os.path.join(path, executable)

    return None


class _Result:
    class Status:  # pylint: disable=too-few-public-methods
        DONE = 'done'
        SKIPPED = 'skipped'
        FAILED = 'failed'

    def __init__(self, status, *messages):
        self._status = status
        self._messages = list(messages)

    def ok(self):
        return self._status in {_Result.Status.DONE, _Result.Status.SKIPPED}

    def status_str(self):
        return self._status

    def messages(self):
        return self._messages


def _get_env(varname):
    globs = os.environ.get(varname, '').split(os.pathsep)
    unique_globs = []
    for pat in globs:
        if pat and pat not in unique_globs:
            unique_globs.append(pat)

    files = []
    warnings = []
    for pat in unique_globs:
        if pat:
            matches = glob.glob(pat)
            if not matches:
                warnings.append(
                    'warning: pattern "{}" in {} matched 0 files'.format(
                        pat, varname))
            files.extend(matches)

    if not files:
        warnings.append('warning: variable {} matched 0 files'.format(varname))

    return files, warnings


def result_func(glob_warnings):
    def result(status, *args):
        return _Result(status, *([str(x) for x in glob_warnings] + list(args)))

    return result


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
            ('Host tools', self.host_tools),
        ]

        # TODO(pwbug/63): Add a Windows version of cargo to CIPD.
        if not self._is_windows and os.environ.get('PW_CARGO_SETUP', ''):
            steps.append(("Rust cargo", self.cargo))

        self._log(
            Color.bold('Downloading and installing packages into local '
                       'source directory:\n'))

        max_name_len = max(len(name) for name, _ in steps)

        self._env.comment('''
This file is automatically generated. DO NOT EDIT!
For details, see $PW_ROOT/pw_env_setup/py/pw_env_setup/env_setup.py and
$PW_ROOT/pw_env_setup/py/pw_env_setup/environment.py.
'''.strip())

        if not self._is_windows:
            self._env.comment('''
For help debugging errors in this script, uncomment the next line.
set -x
Then use `set +x` to go back to normal.
'''.strip())

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

            spin = spinner.Spinner()
            with spin():
                result = step()

            self._log(result.status_str())

            self._env.echo(result.status_str())
            for message in result.messages():
                sys.stderr.write('{}\n'.format(message))
                self._env.echo(message)

            if not result.ok():
                return -1

        self._log('')
        self._env.echo('')

        self._env.hash()

        self._env.echo(Color.bold('Sanity checking the environment:'))
        self._env.echo()

        log_level = 'warn' if 'PW_ENVSETUP_QUIET' in os.environ else 'info'
        doctor = ['pw', '--no-banner', '--loglevel', log_level, 'doctor']

        self._env.command(doctor)
        self._env.echo()

        self._env.echo(
            Color.bold('Environment looks good, you are ready to go!'))
        self._env.echo()

        with open(self._shell_file, 'w') as outs:
            self._env.write(outs)

        return 0

    def cipd(self):
        install_dir = os.path.join(self._pw_root, '.cipd')

        cipd_client = cipd_wrapper.init(install_dir, silent=True)

        package_files, glob_warnings = _get_env('PW_CIPD_PACKAGE_FILES')
        result = result_func(glob_warnings)

        if not package_files:
            return result(_Result.Status.SKIPPED)

        if not cipd_update.update(cipd=cipd_client,
                                  root_install_dir=install_dir,
                                  package_files=package_files,
                                  cache_dir=self._cipd_cache_dir,
                                  env_vars=self._env):
            return result(_Result.Status.FAILED)

        return result(_Result.Status.DONE)

    def virtualenv(self):
        """Setup virtualenv."""

        venv_path = os.path.join(self._pw_root, '.python3-env')

        requirements, req_glob_warnings = _get_env(
            'PW_VIRTUALENV_REQUIREMENTS')
        setup_py_roots, setup_glob_warnings = _get_env(
            'PW_VIRTUALENV_SETUP_PY_ROOTS')
        result = result_func(req_glob_warnings + setup_glob_warnings)

        orig_python3 = _which('python3')
        with self._env():
            new_python3 = _which('python3')

        # There is an issue with the virtualenv module on Windows where it
        # expects sys.executable to be called "python.exe" or it fails to
        # properly execute. If we installed Python 3 in the CIPD step we need
        # to address this. Detect if we did so and if so create a copy of
        # python3.exe called python.exe so that virtualenv works.
        if orig_python3 != new_python3 and self._is_windows:
            python3_copy = os.path.join(os.path.dirname(new_python3),
                                        'python.exe')
            if not os.path.exists(python3_copy):
                shutil.copyfile(new_python3, python3_copy)
            new_python3 = python3_copy

        if not requirements and not setup_py_roots:
            return result(_Result.Status.SKIPPED)

        if not virtualenv_setup.install(venv_path=venv_path,
                                        requirements=requirements,
                                        setup_py_roots=setup_py_roots,
                                        python=new_python3,
                                        env=self._env):
            return result(_Result.Status.FAILED)

        return result(_Result.Status.DONE)

    def host_tools(self):
        # The host tools are grabbed from CIPD, at least initially. If the
        # user has a current host build, that build will be used instead.
        # TODO(mohrr) find a way to do stuff like this for all projects.
        host_dir = os.path.join(self._pw_root, 'out', 'host')
        self._env.prepend('PATH', os.path.join(host_dir, 'host_tools'))
        return _Result(_Result.Status.DONE)

    def cargo(self):
        if not os.environ.get('PW_CARGO_SETUP', ''):
            return _Result(
                _Result.Status.SKIPPED,
                '    Note: Re-run bootstrap with PW_CARGO_SETUP=1 set '
                'in your environment',
                '          to enable Rust. (Rust is usually not needed.)',
            )

        package_files, glob_warnings = _get_env('PW_CARGO_PACKAGE_FILES')
        result = result_func(glob_warnings)

        if not package_files:
            return result(_Result.Status.SKIPPED)

        if not cargo_setup.install(pw_root=self._pw_root,
                                   package_files=package_files,
                                   env=self._env):
            return result(_Result.Status.FAILED)

        return result(_Result.Status.DONE)


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
