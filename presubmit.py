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
"""Runs the Pigweed local presubmit checks."""

import argparse
import os
import re
import shutil
import sys

from pw_presubmit.presubmit_tools import call, filter_paths, PresubmitFailure
from pw_presubmit import format_cc, presubmit_tools


def _init_cipd():
    cipd = os.path.abspath('.presubmit/cipd')
    call(sys.executable, 'env_setup/cipd/update.py', '--install-dir', cipd)
    os.environ['PATH'] = os.pathsep.join((
        cipd,
        os.path.join(cipd, 'bin'),
        os.environ['PATH'],
    ))
    print('PATH', os.environ['PATH'])


def _init_virtualenv():
    """Set up virtualenv, assumes recent Python 3 is already installed."""
    venv = os.path.abspath('.presubmit/venv')
    if not os.path.isdir(venv):
        call('python', '-m', 'venv', venv)
    os.environ['PATH'] = os.pathsep.join((
        os.path.join(venv, 'bin'),
        os.environ['PATH'],
    ))

    call('python', '-m', 'pip', 'install', '--upgrade', 'pip')
    call('python', '-m', 'pip', 'install',
         '--log', os.path.join(venv, 'pip.log'),
         '-r', 'env_setup/virtualenv/requirements.txt')  # yapf: disable


def init():
    _init_cipd()
    _init_virtualenv()


presubmit_dir = lambda *paths: os.path.join('.presubmit', *paths)


#
# GN presubmit checks
#
def gn_args(**kwargs):
    return '--args=' + ' '.join(f'{arg}={val}' for arg, val in kwargs.items())


GN_GEN = 'gn', 'gen', '--color=always', '--check'


@filter_paths(endswith=['.gn', '.gni'])
def gn_format(paths):
    call('gn', 'format', '--dry-run', *paths)


def gn_clang_build():
    call(
        *GN_GEN, '--export-compile-commands', presubmit_dir('clang'),
        gn_args(pw_target_config='"//targets/host/host.gni"',
                pw_target_toolchain='"//pw_toolchain:host_clang_os"'))
    call('ninja', '-C', presubmit_dir('clang'))


def gn_gcc_build():
    call(
        *GN_GEN, presubmit_dir('gcc'),
        gn_args(pw_target_config='"//targets/host/host.gni"',
                pw_target_toolchain='"//pw_toolchain:host_gcc_os"'))
    call('ninja', '-C', presubmit_dir('gcc'))


def gn_arm_build():
    call(
        *GN_GEN, presubmit_dir('arm'),
        gn_args(
            pw_target_config='"//targets/stm32f429i-disc1/target_config.gni"'))
    call('ninja', '-C', presubmit_dir('arm'))


GN = (
    gn_format,
    gn_clang_build,
    gn_gcc_build,
    gn_arm_build,
)


#
# C++ presubmit checks
#
@filter_paths(endswith=format_cc.SOURCE_EXTENSIONS)
def clang_format(paths):
    if format_cc.check_format(paths):
        raise PresubmitFailure


@filter_paths(endswith=format_cc.SOURCE_EXTENSIONS)
def clang_tidy(paths):
    if not os.path.exists(presubmit_dir('clang', 'compile_commands.json')):
        raise PresubmitFailure('clang_tidy MUST be run after generating '
                               'compile_commands.json in a clang build!')

    call('clang-tidy', f'-p={presubmit_dir("clang")}', *paths)


CC = (
    presubmit_tools.pragma_once,
    clang_format,
    # TODO(hepler): Enable clang-tidy when it passes.
    # clang_tidy,
)


#
# Python presubmit checks
#
@filter_paths(endswith='.py')
def pylint_errors(paths):
    call(sys.executable, '-m', 'pylint', '-E', *paths)


@filter_paths(endswith='.py')
def yapf(paths):
    from yapf.yapflib.yapf_api import FormatFile

    errors = []

    for path in paths:
        diff, _, changed = FormatFile(path, print_diff=True, in_place=False)
        if changed:
            errors.append(path)
            print(format_cc.colorize_diff(diff))

    if errors:
        print(f'--> Files with formatting errors: {len(errors)}')
        print('   ', '\n    '.join(errors))
        raise PresubmitFailure


@filter_paths(endswith='.py', exclude=r'(?:.+/)?setup\.py')
def mypy(paths):
    import mypy.api as mypy_api

    report, errors, exit_status = mypy_api.run(paths)
    if exit_status:
        print(errors)
        print(report)
        raise PresubmitFailure


PYTHON = (
    # TODO(hepler): Enable yapf, mypy, and pylint when they pass.
    # pylint_errors,
    # yapf,
    # mypy,
)


#
# Bazel presubmit checks
#
@filter_paths(endswith=format_cc.SOURCE_EXTENSIONS)
def bazel_test(unused_paths):
    prefix = '.presubmit/bazel-'
    call('bazel', 'build', '//...', '--symlink_prefix', prefix)
    call('bazel', 'test', '//...', '--symlink_prefix', prefix)


BAZEL = (bazel_test, )

#
# General presubmit checks
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
    r'(?:.+/)?requirements.in',
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
        print('-->', presubmit_tools.plural(errors, 'file'),
              'with a missing or incorrect copyright notice:')
        print('   ', '\n    '.join(errors))
        raise PresubmitFailure


GENERAL = (copyright_notice, )

#
# Presubmit check programs
#
QUICK_PRESUBMIT = (
    *GENERAL,
    *PYTHON,
    gn_format,
    gn_clang_build,
    presubmit_tools.pragma_once,
    clang_format,
)

PROGRAMS = {
    'full': GN + CC + PYTHON + BAZEL + GENERAL,
    'quick': QUICK_PRESUBMIT,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--clean',
        action='store_true',
        help='Deletes the .presubmit directory before starting')
    parser.add_argument(
        '--skip_init',
        action='store_true',
        help='Clone the buildtools to prior to running the checks')
    parser.add_argument('-p',
                        '--program',
                        choices=PROGRAMS,
                        default='full',
                        help='Which presubmit program to run')

    presubmit_tools.add_parser_arguments(parser)

    args = parser.parse_args()

    if args.clean and os.path.exists(presubmit_dir()):
        shutil.rmtree(presubmit_dir())

    init_step = () if args.skip_init else (init,)
    program = init_step + PROGRAMS[args.program]

    # Remove custom arguments so we can use args to call run_presubmit.
    del args.clean, args.program, args.skip_init

    return 0 if presubmit_tools.run_presubmit(program, **vars(args)) else 1


if __name__ == '__main__':
    sys.exit(main())
