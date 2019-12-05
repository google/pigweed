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
"""Runs the local presubmit checks for the Pigweed repository."""

import argparse
import logging
import os
import re
import shutil
import sys
from typing import Dict, Sequence

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_presubmit import format_code
from pw_presubmit.install_hook import install_hook
from pw_presubmit import call, filter_paths, PresubmitFailure

_LOG: logging.Logger = logging.getLogger(__name__)


def presubmit_dir(*paths):
    return os.path.join('.presubmit', *paths)


def run_python_module(*args, **kwargs):
    return call('python', '-m', *args, **kwargs)


#
# Initialization
#
def init_cipd():
    cipd = os.path.abspath('.presubmit/cipd')
    call(sys.executable, 'env_setup/cipd/update.py', '--install-dir', cipd)
    os.environ['PATH'] = os.pathsep.join((
        cipd,
        os.path.join(cipd, 'bin'),
        os.environ['PATH'],
    ))
    _LOG.debug('PATH %s', os.environ['PATH'])


@filter_paths(endswith='.py')  # Only run if there are .py files.
def init_virtualenv(unused_paths):
    """Set up virtualenv, assumes recent Python 3 is already installed."""
    venv = os.path.abspath('.presubmit/venv')

    # For speed, don't build the venv if it exists. Use --clean to recreate it.
    if not os.path.isdir(venv):
        call(
            'python3',
            'env_setup/virtualenv/init.py',
            f'--venv_path={venv}',
            '--requirements=env_setup/virtualenv/requirements.txt',
        )

    os.environ['PATH'] = os.pathsep.join((
        os.path.join(venv, 'bin'),
        os.environ['PATH'],
    ))


INIT = (
    init_cipd,
    init_virtualenv,
)


#
# GN presubmit checks
#
def gn_args(**kwargs):
    return '--args=' + ' '.join(f'{arg}={val}' for arg, val in kwargs.items())


GN_GEN = 'gn', 'gen', '--color=always', '--check'


def gn_clang_build():
    call(
        *GN_GEN, '--export-compile-commands', presubmit_dir('clang'),
        gn_args(pw_target_config='"//targets/host/host.gni"',
                pw_target_toolchain='"//pw_toolchain:host_clang_os"'))
    call('ninja', '-C', presubmit_dir('clang'))


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_gcc_build(unused_paths):
    call(
        *GN_GEN, presubmit_dir('gcc'),
        gn_args(pw_target_config='"//targets/host/host.gni"',
                pw_target_toolchain='"//pw_toolchain:host_gcc_os"'))
    call('ninja', '-C', presubmit_dir('gcc'))


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_arm_build(unused_paths):
    call(
        *GN_GEN, presubmit_dir('arm'),
        gn_args(
            pw_target_config='"//targets/stm32f429i-disc1/target_config.gni"'))
    call('ninja', '-C', presubmit_dir('arm'))


GN = (
    gn_clang_build,
    gn_gcc_build,
    gn_arm_build,
)


#
# C++ presubmit checks
#
@filter_paths(endswith=format_code.C_FORMAT.extensions)
def clang_tidy(paths):
    if not os.path.exists(presubmit_dir('clang', 'compile_commands.json')):
        raise PresubmitFailure('clang_tidy MUST be run after generating '
                               'compile_commands.json in a clang build!')

    call('clang-tidy', f'-p={presubmit_dir("clang")}', *paths)


CC = (
    pw_presubmit.pragma_once,
    # TODO(hepler): Enable clang-tidy when it passes.
    # clang_tidy,
)


#
# Python presubmit checks
#
@filter_paths(endswith='.py')
def test_python_packages(paths):
    packages = pw_presubmit.find_python_packages(paths)

    if not packages:
        _LOG.info('No Python packages were found.')
        return

    for package in packages:
        call('python', os.path.join(package, 'setup.py'), 'test')


@filter_paths(endswith='.py')
def pylint_errors_only(paths):
    run_python_module('pylint', '-E', '-j', '0', *paths)


@filter_paths(endswith='.py')
def pylint(paths):
    try:
        run_python_module('pylint', '-j', '0', *paths)
    except PresubmitFailure:
        # TODO(hepler): Enforce pylint when it passes.
        _LOG.warning('pylint checks FAILED!')
        _LOG.warning('Treating this as a warning... for now.')


@filter_paths(endswith='.py', exclude=r'(?:.+/)?setup\.py')
def mypy(paths):
    run_python_module('mypy', *paths)


PYTHON = (
    test_python_packages,
    pylint_errors_only,  # TODO(hepler): Remove this when pylint is passing.
    pylint,
    # TODO(hepler): Enable mypy when it passes.
    # mypy,
)


#
# Bazel presubmit checks
#
@filter_paths(endswith=format_code.C_FORMAT.extensions)
def bazel_test(unused_paths):
    prefix = '.presubmit/bazel-'
    call('bazel', 'build', '//...', '--symlink_prefix', prefix)
    call('bazel', 'test', '//...', '--symlink_prefix', prefix)


BAZEL = (bazel_test, )

#
# Code format presubmit checks
#
COPYRIGHT_FIRST_LINE = re.compile(
    r'^(#|//| \*) Copyright 20\d\d The Pigweed Authors$')

COPYRIGHT_LINES = tuple("""\

 Licensed under the Apache License, Version 2.0 (the "License"); you may not
 use this file except in compliance with the License. You may obtain a copy of
 the License at

     https://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 License for the specific language governing permissions and limitations under
 the License.
""".splitlines(True))

_EXCLUDE_FROM_COPYRIGHT_NOTICE = (
    r'(?:.+/)?\..+',
    r'AUTHORS',
    r'LICENSE',
    r'.*\.md',
    r'.*\.rst',
    r'(?:.+/)?requirements.txt',
)


@filter_paths(exclude=_EXCLUDE_FROM_COPYRIGHT_NOTICE)
def copyright_notice(paths):
    """Checks that the copyright notice is present."""

    errors = []

    for path in paths:
        with open(path) as file:
            # Skip shebang and blank lines
            line = file.readline()
            while line.startswith(('#!', '/*')) or not line.strip():
                line = file.readline()

            first_line = COPYRIGHT_FIRST_LINE.match(line)
            if not first_line:
                errors.append(path)
                continue

            comment = first_line.group(1)

            for expected, actual in zip(COPYRIGHT_LINES, file):
                if comment + expected != actual:
                    errors.append(path)
                    break

    if errors:
        _LOG.warning('%s with a missing or incorrect copyright notice:\n%s',
                     pw_presubmit.plural(errors, 'file'), '\n'.join(errors))
        raise PresubmitFailure


CODE_FORMAT = (copyright_notice, *format_code.PRESUBMIT_CHECKS)

#
# Presubmit check programs
#
QUICK_PRESUBMIT: Sequence = (
    *INIT,
    *PYTHON,
    gn_clang_build,
    pw_presubmit.pragma_once,
    *CODE_FORMAT,
)

PROGRAMS: Dict[str, Sequence] = {
    'full': INIT + GN + CC + PYTHON + BAZEL + CODE_FORMAT,
    'quick': QUICK_PRESUBMIT,
}


def argument_parser(parser=None) -> argparse.ArgumentParser:
    if parser is None:
        parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '--clean',
        action='store_true',
        help='Deletes the .presubmit directory before starting')

    exclusive = parser.add_mutually_exclusive_group()
    exclusive.add_argument(
        '--install',
        action='store_true',
        help='Installs the presubmit as a Git pre-push hook and exits')
    exclusive.add_argument('-p',
                           '--program',
                           choices=PROGRAMS,
                           default='full',
                           help='Which presubmit program to run')
    pw_presubmit.add_arguments(parser)

    return parser


def main(program: str, clean: bool, install: bool = False,
         **presubmit_args) -> int:
    if clean and os.path.exists(presubmit_dir()):
        shutil.rmtree(presubmit_dir())

    if install:
        install_hook(__file__, 'pre-push', ['--base', 'origin/master'],
                     presubmit_args['repository'])
        return 0

    if pw_presubmit.run_presubmit(PROGRAMS[program], **presubmit_args):
        return 0

    return 1


if __name__ == '__main__':
    # By default, display log messages like a simple print statement.
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    sys.exit(main(**vars(argument_parser().parse_args())))
