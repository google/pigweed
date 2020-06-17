#!/usr/bin/env python3
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
"""Checks if the environment is set up correctly for Pigweed."""

import argparse
import logging
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from typing import Callable, List


def call_stdout(*args, **kwargs):
    kwargs.update(stdout=subprocess.PIPE)
    proc = subprocess.run(*args, **kwargs)
    return proc.stdout.decode('utf-8')


class _Fatal(Exception):
    pass


class DoctorContext:
    """Base class for other checks."""
    def __init__(self, *, strict=False, log=None):
        self.name = self.__class__.__name__
        self._strict = strict
        self._log = log or logging.getLogger(__name__)
        self.failures = set()
        self.curr_checker = None

    def fatal(self, fmt, *args, **kwargs):
        """Same as error() but terminates the checkearly."""
        self.error(fmt, *args, **kwargs)
        raise _Fatal()

    def error(self, fmt, *args, **kwargs):
        self._log.error(fmt, *args, **kwargs)
        self.failures.add(self.curr_checker)

    def warning(self, fmt, *args, **kwargs):
        if self._strict:
            self.error(fmt, *args, **kwargs)
        else:
            self._log.warning(fmt, *args, **kwargs)

    def info(self, fmt, *args, **kwargs):
        self._log.info(fmt, *args, **kwargs)

    def debug(self, fmt, *args, **kwargs):
        self._log.debug(fmt, *args, **kwargs)


def register_into(dest):
    def decorate(func):
        dest.append(func)
        return func

    return decorate


CHECKS: List[Callable] = []


@register_into(CHECKS)
def pw_root(ctx: DoctorContext):
    """Check that environment variable PW_ROOT is set and makes sense."""
    try:
        root = pathlib.Path(os.environ['PW_ROOT']).resolve()
    except KeyError:
        ctx.fatal('PW_ROOT not set')

    git_root = pathlib.Path(
        call_stdout(['git', 'rev-parse', '--show-toplevel'], cwd=root).strip())
    git_root = git_root.resolve()
    if root != git_root:
        ctx.error('PW_ROOT (%s) != `git rev-parse --show-toplevel` (%s)', root,
                  git_root)


@register_into(CHECKS)
def git_hook(ctx: DoctorContext):
    """Check that presubmit git hook is installed."""
    if not os.environ.get('PW_ENABLE_PRESUBMIT_HOOK_WARNING'):
        return

    try:
        root = pathlib.Path(os.environ['PW_ROOT'])
    except KeyError:
        return  # This case is handled elsewhere.

    hook = root / '.git' / 'hooks' / 'pre-push'
    if not os.path.isfile(hook):
        ctx.info('Presubmit hook not installed, please run '
                 "'pw presubmit --install' before pushing changes.")


@register_into(CHECKS)
def python_version(ctx: DoctorContext):
    """Check the Python version is correct."""
    actual = sys.version_info
    expected = (3, 8)
    if actual[0:2] < expected or actual[0] != expected[0]:
        ctx.error('Python %d.%d.x required, got Python %d.%d.%d', *expected,
                  *actual[0:3])
    elif actual[0:2] > expected:
        ctx.warning('Python %d.%d.x required, got Python %d.%d.%d', *expected,
                    *actual[0:3])


@register_into(CHECKS)
def virtualenv(ctx: DoctorContext):
    """Check that we're in the correct virtualenv."""
    try:
        venv_path = pathlib.Path(os.environ['VIRTUAL_ENV']).resolve()
    except KeyError:
        ctx.error('VIRTUAL_ENV not set')
        return

    # When running in LUCI we might not have gone through the normal environment
    # setup process, so we need to skip the rest of this step.
    if 'LUCI_CONTEXT' in os.environ:
        return

    root = pathlib.Path(os.environ['PW_ROOT']).resolve()

    if root not in venv_path.parents:
        ctx.error('VIRTUAL_ENV (%s) not inside PW_ROOT (%s)', venv_path, root)
        ctx.error('\n'.join(os.environ.keys()))


@register_into(CHECKS)
def cipd(ctx: DoctorContext):
    """Check cipd is set up correctly and in use."""
    cipd_path = 'pigweed'

    cipd_exe = shutil.which('cipd')
    if not cipd_exe:
        ctx.fatal('cipd not in PATH (%s)', os.environ['PATH'])

    temp = tempfile.NamedTemporaryFile(prefix='cipd', delete=False)
    subprocess.run(['cipd', 'acl-check', '-json-output', temp.name, cipd_path],
                   stdout=subprocess.PIPE)
    if not json.load(temp)['result']:
        ctx.fatal(
            "can't access %s CIPD directory, have you run "
            "'cipd auth-login'?", cipd_path)

    commands_expected_from_cipd = [
        'arm-none-eabi-gcc',
        'gn',
        'ninja',
        'protoc',
    ]

    # TODO(mohrr) get these tools in CIPD for Windows.
    if os.name == 'posix':
        commands_expected_from_cipd += [
            'bazel',
            'bloaty',
            'clang++',
        ]

    for command in commands_expected_from_cipd:
        path = shutil.which(command)
        if path is None:
            ctx.error('could not find %s in PATH (%s)', command,
                      os.environ['PATH'])
        elif 'cipd' not in path:
            ctx.warning('not using %s from cipd, got %s (path is %s)', command,
                        path, os.environ['PATH'])


@register_into(CHECKS)
def cipd_versions(ctx: DoctorContext):
    """Check cipd is set up correctly and in use."""
    try:
        root = pathlib.Path(os.environ['PW_ROOT']).resolve()
    except KeyError:
        return  # This case is handled elsewhere.

    versions_path = root.joinpath('.cipd', 'pigweed', '.versions')
    # Deliberately not checking luci.json--it's not required to be up-to-date.
    json_path = root.joinpath('pw_env_setup', 'py', 'pw_env_setup',
                              'cipd_setup', 'pigweed.json')

    with json_path.open() as ins:
        packages = json.load(ins)

    for package in packages:
        ctx.debug('checking version of %s', package['path'])
        name = [
            part for part in package['path'].split('/') if '{' not in part
        ][-1]
        path = versions_path.joinpath(f'{name}.cipd_version')
        if not path.is_file():
            ctx.debug('no version file')
            continue

        with path.open() as ins:
            installed = json.load(ins)

        describe = (
            'cipd',
            'describe',
            installed['package_name'],
            '-version',
            installed['instance_id'],
        )
        ctx.debug('%s', ' '.join(describe))
        output_raw = subprocess.check_output(describe).decode()
        ctx.debug('output: %r', output_raw)
        output = output_raw.split()

        for tag in package['tags']:
            if tag not in output:
                ctx.error(
                    'CIPD package %s is out of date, please rerun bootstrap',
                    installed['package_name'])


def doctor(strict=False, checks=None):
    """Run all the Check subclasses defined in this file."""

    ctx = DoctorContext(strict=strict)

    if checks is None:
        checks = tuple(CHECKS)

    ctx.debug('Doctor running %d checks...', len(checks))
    for check in checks:
        try:
            ctx.debug('Running %s...', check.__name__)
            ctx.curr_checker = check.__name__
            check(ctx)

        except _Fatal:
            pass

        finally:
            ctx.curr_checker = None

    if ctx.failures:
        ctx.info('Failed checks: %s', ', '.join(ctx.failures))
    else:
        ctx.info('Environment passes all checks!')
    return len(ctx.failures)


def main() -> int:
    """Check that the environment is set up correctly for Pigweed."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--strict',
        action='store_true',
        help='Run additional checks.',
    )

    return doctor(**vars(parser.parse_args()))


if __name__ == '__main__':
    # By default, display log messages like a simple print statement.
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    sys.exit(main())
