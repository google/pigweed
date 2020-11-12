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
import json
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
    old_sys_path = copy.deepcopy(sys.path)
    filename = None
    if hasattr(sys.modules[__name__], '__file__'):
        filename = __file__
    else:
        # Try introspection in environments where __file__ is not populated.
        frame = inspect.currentframe()
        if frame is not None:
            filename = inspect.getfile(frame)
    # If none of our strategies worked, we're in a strange runtime environment.
    # The imports are almost certainly going to fail.
    if filename is None:
        raise RuntimeError(
            'Unable to locate pw_env_setup module; cannot continue.\n'
            '\n'
            'Try updating to one of the standard Python implemetations:\n'
            '  https://www.python.org/downloads/')
    sys.path = [
        os.path.abspath(os.path.join(filename, os.path.pardir, os.path.pardir))
    ]
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
from pw_env_setup import windows_env_start


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
    class Status:
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


def _process_globs(globs):
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
                    'warning: pattern "{}" matched 0 files'.format(pat))
            files.extend(matches)

    if globs and not files:
        warnings.append('warning: matched 0 total files')

    return files, warnings


def result_func(glob_warnings):
    def result(status, *args):
        return _Result(status, *([str(x) for x in glob_warnings] + list(args)))

    return result


# TODO(mohrr) remove disable=useless-object-inheritance once in Python 3.
# pylint: disable=useless-object-inheritance
# pylint: disable=too-many-instance-attributes
# pylint: disable=too-many-arguments
class EnvSetup(object):
    """Run environment setup for Pigweed."""
    def __init__(self, pw_root, cipd_cache_dir, shell_file, quiet, install_dir,
                 use_pigweed_defaults, cipd_package_file, virtualenv_root,
                 virtualenv_requirements, virtualenv_gn_target,
                 cargo_package_file, enable_cargo, json_file, project_root):
        self._env = environment.Environment()
        self._project_root = project_root
        self._pw_root = pw_root
        self._setup_root = os.path.join(pw_root, 'pw_env_setup', 'py',
                                        'pw_env_setup')
        self._cipd_cache_dir = cipd_cache_dir
        self._shell_file = shell_file
        self._is_windows = os.name == 'nt'
        self._quiet = quiet
        self._install_dir = install_dir
        self._virtualenv_root = (virtualenv_root
                                 or os.path.join(install_dir, 'pigweed-venv'))

        if os.path.isfile(shell_file):
            os.unlink(shell_file)

        if isinstance(self._pw_root, bytes) and bytes != str:
            self._pw_root = self._pw_root.decode()

        self._cipd_package_file = []
        self._virtualenv_requirements = []
        self._virtualenv_gn_targets = []
        self._cargo_package_file = []
        self._enable_cargo = enable_cargo

        self._json_file = json_file

        setup_root = os.path.join(pw_root, 'pw_env_setup', 'py',
                                  'pw_env_setup')

        # TODO(pwbug/67, pwbug/68) Investigate pulling these files into an
        # oxidized env setup executable instead of referring to them in the
        # source tree. Note that this could be error-prone because users expect
        # changes to the files in the source tree to affect bootstrap.
        if use_pigweed_defaults:
            # If updating this section make sure to update
            # $PW_ROOT/pw_env_setup/docs.rst as well.
            self._cipd_package_file.append(
                os.path.join(setup_root, 'cipd_setup', 'pigweed.json'))
            self._cipd_package_file.append(
                os.path.join(setup_root, 'cipd_setup', 'luci.json'))
            self._virtualenv_requirements.append(
                os.path.join(setup_root, 'virtualenv_setup',
                             'requirements.txt'))
            self._virtualenv_gn_targets.append(
                virtualenv_setup.GnTarget(
                    '{}#:python.install'.format(pw_root)))
            self._cargo_package_file.append(
                os.path.join(setup_root, 'cargo_setup', 'packages.txt'))

        self._cipd_package_file.extend(cipd_package_file)
        self._virtualenv_requirements.extend(virtualenv_requirements)
        self._virtualenv_gn_targets.extend(virtualenv_gn_target)
        self._cargo_package_file.extend(cargo_package_file)

        self._env.set('PW_PROJECT_ROOT', project_root)
        self._env.set('PW_ROOT', pw_root)
        self._env.set('_PW_ACTUAL_ENVIRONMENT_ROOT', install_dir)
        self._env.add_replacement('_PW_ACTUAL_ENVIRONMENT_ROOT', install_dir)
        self._env.add_replacement('PW_ROOT', pw_root)

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

        if os.name == 'nt':
            windows_env_start.print_banner(bootstrap=True, no_shell_file=False)
        else:
            enable_colors()

        steps = [
            ('CIPD package manager', self.cipd),
            ('Python environment', self.virtualenv),
            ('Host tools', self.host_tools),
        ]

        # TODO(pwbug/63): Add a Windows version of cargo to CIPD.
        if not self._is_windows and self._enable_cargo:
            steps.append(("Rust cargo", self.cargo))

        if self._is_windows:
            steps.append(("Windows scripts", self.win_scripts))

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

        self._env.finalize()

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

        deactivate = os.path.join(
            self._install_dir,
            'deactivate{}'.format(os.path.splitext(self._shell_file)[1]))
        with open(deactivate, 'w') as outs:
            self._env.write_deactivate(outs)

        config = {
            # Skipping sysname and nodename in os.uname(). nodename could change
            # based on the current network. sysname won't change, but is
            # redundant because it's contained in release or version, and
            # skipping it here simplifies logic.
            'uname': ' '.join(getattr(os, 'uname', lambda: ())()[2:]),
            'os': os.name,
        }

        with open(os.path.join(self._install_dir, 'config.json'), 'w') as outs:
            outs.write(
                json.dumps(config, indent=4, separators=(',', ': ')) + '\n')

        if self._json_file is not None:
            with open(self._json_file, 'w') as outs:
                self._env.json(outs)

        return 0

    def cipd(self):
        install_dir = os.path.join(self._install_dir, 'cipd')

        cipd_client = cipd_wrapper.init(install_dir, silent=True)

        package_files, glob_warnings = _process_globs(self._cipd_package_file)
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

        requirements, req_glob_warnings = _process_globs(
            self._virtualenv_requirements)
        result = result_func(req_glob_warnings)

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

        if not requirements and not self._virtualenv_gn_targets:
            return result(_Result.Status.SKIPPED)

        if not virtualenv_setup.install(
                project_root=self._project_root,
                venv_path=self._virtualenv_root,
                requirements=requirements,
                gn_targets=self._virtualenv_gn_targets,
                python=new_python3,
                env=self._env,
        ):
            return result(_Result.Status.FAILED)

        return result(_Result.Status.DONE)

    def host_tools(self):
        # The host tools are grabbed from CIPD, at least initially. If the
        # user has a current host build, that build will be used instead.
        # TODO(mohrr) find a way to do stuff like this for all projects.
        host_dir = os.path.join(self._pw_root, 'out', 'host')
        self._env.prepend('PATH', os.path.join(host_dir, 'host_tools'))
        return _Result(_Result.Status.DONE)

    def win_scripts(self):
        # These scripts act as a compatibility layer for windows.
        env_setup_dir = os.path.join(self._pw_root, 'pw_env_setup')
        self._env.prepend('PATH', os.path.join(env_setup_dir,
                                               'windows_scripts'))
        return _Result(_Result.Status.DONE)

    def cargo(self):
        install_dir = os.path.join(self._install_dir, 'cargo')

        package_files, glob_warnings = _process_globs(self._cargo_package_file)
        result = result_func(glob_warnings)

        if not package_files:
            return result(_Result.Status.SKIPPED)

        if not cargo_setup.install(install_dir=install_dir,
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

    project_root = os.environ.get('PW_PROJECT_ROOT', None) or pw_root

    parser.add_argument(
        '--project-root',
        default=project_root,
        required=not project_root,
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

    parser.add_argument(
        '--install-dir',
        help='Location to install environment.',
        required=True,
    )

    parser.add_argument(
        '--use-pigweed-defaults',
        help='Use Pigweed default values in addition to the given environment '
        'variables.',
        action='store_true',
    )

    parser.add_argument(
        '--cipd-package-file',
        help='CIPD package file. JSON file consisting of a list of dicts with '
        '"path" and "tags" keys, where "tags" a list of str.',
        default=[],
        action='append',
    )

    parser.add_argument(
        '--virtualenv-requirements',
        help='Pip requirements file. Compiled with pip-compile.',
        default=[],
        action='append',
    )

    parser.add_argument(
        '--virtualenv-gn-target',
        help=('GN targets that build and install Python packages. Format: '
              "path/to/gn_root#target"),
        default=[],
        action='append',
        type=virtualenv_setup.GnTarget,
    )

    parser.add_argument(
        '--virtualenv-root',
        help=('Root of virtualenv directory. Default: '
              '<install_dir>/pigweed-venv'),
        default=None,
    )

    parser.add_argument(
        '--cargo-package-file',
        help='Rust cargo packages to install. Lines with package name and '
        'version separated by a space.',
        default=[],
        action='append',
    )

    parser.add_argument(
        '--enable-cargo',
        help='Enable cargo installation.',
        action='store_true',
    )

    parser.add_argument(
        '--json-file',
        help='Dump environment variable operations to a JSON file.',
        default=None,
    )

    args = parser.parse_args(argv)

    one_required = (
        'use_pigweed_defaults',
        'cipd_package_file',
        'virtualenv_requirements',
        'virtualenv_gn_target',
        'cargo_package_file',
    )

    if not any(getattr(args, x) for x in one_required):
        parser.error('At least one of ({}) is required'.format(', '.join(
            '"--{}"'.format(x.replace('_', '-')) for x in one_required)))

    return args


def main():
    try:
        return EnvSetup(**vars(parse())).setup()
    except subprocess.CalledProcessError as err:
        print()
        print(err.output)
        raise


if __name__ == '__main__':
    sys.exit(main())
