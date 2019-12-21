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
from pathlib import Path
import re
import shutil
import subprocess
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

from pw_presubmit import format_code, PresubmitContext
from pw_presubmit.install_hook import install_hook
from pw_presubmit import call, filter_paths, log_run, plural, PresubmitFailure

_LOG = logging.getLogger(__name__)


def run_python_module(*args, **kwargs):
    return call('python', '-m', *args, **kwargs)


#
# Initialization
#
def init_cipd(ctx: PresubmitContext):
    call(sys.executable,
         ctx.repository_root.joinpath('env_setup/cipd/update.py'),
         '--install-dir', ctx.output_directory)

    paths = [ctx.output_directory, ctx.output_directory.joinpath('bin')]
    for base in ctx.output_directory.glob('*'):
        paths.append(base)
        paths.append(base.joinpath('bin'))

    paths.append(Path(os.environ['PATH']))

    os.environ['PATH'] = os.pathsep.join(str(x) for x in paths)
    _LOG.debug('PATH %s', os.environ['PATH'])


def init_virtualenv(ctx: PresubmitContext):
    """Set up virtualenv, assumes recent Python 3 is already installed."""
    virtualenv_source = ctx.repository_root.joinpath('env_setup/virtualenv')

    # For speed, don't build the venv if it exists. Use --clean to recreate it.
    if not ctx.output_directory.joinpath('pyvenv.cfg').is_file():
        call(
            'python3',
            virtualenv_source.joinpath('init.py'),
            f'--venv_path={ctx.output_directory}',
            '--requirements={}'.format(
                virtualenv_source.joinpath('requirements.txt')),
        )

    os.environ['PATH'] = os.pathsep.join((
        str(ctx.output_directory.joinpath('bin')),
        os.environ['PATH'],
    ))


INIT = (
    init_cipd,
    init_virtualenv,
)


#
# GN presubmit checks
#
def gn_args(**kwargs) -> str:
    return '--args=' + ' '.join(f'{arg}={val}' for arg, val in kwargs.items())


def gn_gen(*args, ctx: PresubmitContext, path=None, **kwargs) -> None:
    call('gn',
         'gen',
         path or ctx.output_directory,
         '--color=always',
         '--check',
         *args,
         cwd=ctx.repository_root,
         **kwargs)


def ninja(ctx: PresubmitContext, **kwargs) -> None:
    call('ninja',
         '-C',
         ctx.output_directory,
         cwd=ctx.repository_root,
         **kwargs)


_CLANG_GEN_ARGS = gn_args(pw_target_config='"//targets/host/host.gni"',
                          pw_target_toolchain='"//pw_toolchain:host_clang_os"')


def gn_clang_build(ctx: PresubmitContext):
    gn_gen('--export-compile-commands', _CLANG_GEN_ARGS, ctx=ctx)
    ninja(ctx=ctx)


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_gcc_build(ctx: PresubmitContext):
    gn_gen(gn_args(pw_target_config='"//targets/host/host.gni"',
                   pw_target_toolchain='"//pw_toolchain:host_gcc_os"'),
           ctx=ctx)
    ninja(ctx=ctx)


_ARM_GEN_ARGS = gn_args(
    pw_target_config='"//targets/stm32f429i-disc1/target_config.gni"')


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_arm_build(ctx: PresubmitContext):
    gn_gen(_ARM_GEN_ARGS, ctx=ctx)
    ninja(ctx=ctx)


GN = (
    gn_clang_build,
    gn_gcc_build,
    gn_arm_build,
)


#
# C++ presubmit checks
#
@filter_paths(endswith=format_code.C_FORMAT.extensions)
def clang_tidy(ctx: PresubmitContext):
    # TODO(mohrr) should this check just do a new clang build?
    out = ctx.output_directory.joinpath('..', 'gn_clang_build')
    if not out.joinpath('compile_commands.json').exists():
        raise PresubmitFailure('clang_tidy MUST be run after generating '
                               'compile_commands.json in a clang build!')

    call('clang-tidy', f'-p={out}', *ctx.paths)


CC = (
    pw_presubmit.pragma_once,
    # TODO(hepler): Enable clang-tidy when it passes.
    # clang_tidy,
)


#
# Python presubmit checks
#
@filter_paths(endswith='.py')
def test_python_packages(ctx: PresubmitContext):
    packages = pw_presubmit.find_python_packages(ctx.paths,
                                                 repo=ctx.repository_root)

    if not packages:
        _LOG.info('No Python packages were found.')
        return

    for package in packages:
        call('python', os.path.join(package, 'setup.py'), 'test', cwd=package)


@filter_paths(endswith='.py')
def pylint(ctx: PresubmitContext):
    disable_checkers = [
        # BUG(pwbug/22): Hanging indent check conflicts with YAPF 0.29. For
        # now, use YAPF's version even if Pylint is doing the correct thing
        # just to keep operations simpler. When YAPF upstream fixes the issue,
        # delete this code.
        #
        # See also: https://github.com/google/yapf/issues/781
        'bad-continuation',
    ]
    run_python_module(
        'pylint',
        '--jobs=0',
        f'--disable={",".join(disable_checkers)}',
        *ctx.paths,
        cwd=ctx.repository_root,
    )


@filter_paths(endswith='.py', exclude=r'(?:.+/)?setup\.py')
def mypy(ctx: PresubmitContext):
    run_python_module('mypy', *ctx.paths)


PYTHON = (
    test_python_packages,
    pylint,
    # TODO(hepler): Enable mypy when it passes.
    # mypy,
)


#
# Bazel presubmit checks
#
@filter_paths(endswith=format_code.C_FORMAT.extensions)
def bazel_test(ctx: PresubmitContext):
    call('bazel',
         'test',
         '//...',
         '--verbose_failures',
         '--verbose_explanations',
         '--worker_verbose',
         '--symlink_prefix',
         ctx.output_directory.joinpath('bazel-'),
         cwd=ctx.repository_root)


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

_EXCLUDE_FROM_COPYRIGHT_NOTICE: Sequence[str] = (
    r'(?:.+/)?\..+',
    r'AUTHORS',
    r'LICENSE',
    r'.*\.md',
    r'.*\.rst',
    r'(?:.+/)?requirements.txt',
    r'(.+/)?go.(mod|sum)',
)


@filter_paths(exclude=_EXCLUDE_FROM_COPYRIGHT_NOTICE)
def copyright_notice(ctx: PresubmitContext):
    """Checks that the copyright notice is present."""

    errors = []

    for path in ctx.paths:
        _LOG.debug('Checking %s', path)
        with open(path) as file:
            # Skip shebang and blank lines
            line = file.readline()
            while line.startswith(('#!', '/*')) or not line.strip():
                line = file.readline()

            first_line = COPYRIGHT_FIRST_LINE.match(line)
            if not first_line:
                _LOG.debug('%s: invalid first line %r', path, line)
                errors.append(path)
                continue

            comment = first_line.group(1)

            for expected, actual in zip(COPYRIGHT_LINES, file):
                if comment + expected != actual:
                    _LOG.debug('  bad line: %r', actual)
                    _LOG.debug('  expected: %r', comment + expected)
                    errors.append(path)
                    break

    if errors:
        _LOG.warning('%s with a missing or incorrect copyright notice:\n%s',
                     pw_presubmit.plural(errors, 'file'),
                     '\n'.join(str(e) for e in errors))
        raise PresubmitFailure


CODE_FORMAT = (copyright_notice, *format_code.PRESUBMIT_CHECKS)

#
# General presubmit checks
#


def _get_paths_from_command(*args, ctx: PresubmitContext, **kwargs):
    """Runs a command and reads Bazel or GN //-style paths from it."""
    process = log_run(*args,
                      stdout=subprocess.PIPE,
                      stderr=subprocess.DEVNULL,
                      cwd=ctx.repository_root,
                      **kwargs)
    files = set()

    for line in process.stdout.splitlines():
        path = line.strip().lstrip(b'/').replace(b':', b'/').decode()
        path = ctx.repository_root.joinpath(path)
        if path.is_file():
            _LOG.debug('Found file %s', path)
            files.add(path)

    return files


_SOURCES_IN_BUILD = '.rst', *format_code.C_FORMAT.extensions


@filter_paths(endswith=(*_SOURCES_IN_BUILD, 'BUILD', '.bzl', '.gn', '.gni'))
def source_is_in_build_files(ctx: PresubmitContext):
    """Checks that source files are in the GN and Bazel builds."""

    # Collect all paths in the Bazel build.
    build_bazel = _get_paths_from_command('bazel',
                                          'query',
                                          'kind("source file", //...:*)',
                                          ctx=ctx)

    # Collect all paths in the ARM and Clang GN builds.
    arm_dir = ctx.output_directory.joinpath('arm')
    gn_gen(_ARM_GEN_ARGS, ctx=ctx, path=arm_dir)
    build_gn = _get_paths_from_command('gn', 'desc', arm_dir, '*', ctx=ctx)

    clang_dir = ctx.output_directory.joinpath('clang')
    gn_gen(_CLANG_GEN_ARGS, ctx=ctx, path=clang_dir)
    build_gn.update(
        _get_paths_from_command('gn', 'desc', clang_dir, '*', ctx=ctx))

    missing_bazel = []
    missing_gn = []

    for path in (p for p in ctx.paths if p.suffix in _SOURCES_IN_BUILD):
        if path.suffix != '.rst' and path not in build_bazel:
            missing_bazel.append(path)
        if path not in build_gn:
            missing_gn.append(path)

    if missing_bazel or missing_gn:
        for build, files in [('Bazel', missing_bazel), ('GN', missing_gn)]:
            if files:
                _LOG.warning('%s are missing from the %s build:\n%s',
                             plural(files, 'file'), build,
                             '\n'.join(str(x) for x in files))

        _LOG.warning(
            'All source files must appear in BUILD and BUILD.gn files')
        raise PresubmitFailure


GENERAL = (source_is_in_build_files, )

#
# Presubmit check programs
#
QUICK_PRESUBMIT: Sequence = (
    *INIT,
    *PYTHON,
    gn_clang_build,
    pw_presubmit.pragma_once,
    *CODE_FORMAT,
    *GENERAL,
)

PROGRAMS: Dict[str, Sequence] = {
    'full': INIT + GN + CC + PYTHON + BAZEL + CODE_FORMAT + GENERAL,
    'quick': QUICK_PRESUBMIT,
}


def argument_parser(parser=None) -> argparse.ArgumentParser:
    """Create argument parser."""

    if parser is None:
        parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '-o',
        '--output-directory',
        type=Path,
        help='Output directory (default: <repo root>/.presubmit)',
    )

    parser.add_argument(
        '--clean',
        action='store_true',
        help='Delete the entire output directory before starting.',
    )
    parser.add_argument(
        '--clean-py',
        action='store_true',
        help=('Delete the Python virtualenv in the output directory before '
              'starting.'),
    )

    exclusive = parser.add_mutually_exclusive_group()
    exclusive.add_argument(
        '--install',
        action='store_true',
        help='Install the presubmit as a Git pre-push hook and exits.')
    exclusive.add_argument('-p',
                           '--program',
                           dest='program_name',
                           choices=PROGRAMS,
                           default='full',
                           help='Which presubmit program to run')

    exclusive.add_argument(
        '--step',
        choices=[x.__name__ for x in PROGRAMS['full']],
        action='append',
        help='Provide explicit steps instead of running a predefined program.',
    )

    pw_presubmit.add_arguments(parser)

    return parser


def main(
        program_name: str,
        clean: bool,
        clean_py: bool,
        install: bool,
        repository: Path,
        output_directory: Path,
        step: Sequence[str],
        **presubmit_args,
) -> int:
    """Entry point for presubmit."""

    if not output_directory:
        output_directory = pw_presubmit.git_repo_path('.presubmit',
                                                      repo=repository)
    environment = output_directory
    _LOG.debug('Using environment at %s', environment)

    if clean and environment.exists():
        shutil.rmtree(environment)
    elif clean_py and environment.joinpath('venv').exists():
        shutil.rmtree(environment.joinpath('venv'))

    if install:
        install_hook(__file__, 'pre-push', ['--base', 'origin/master..HEAD'],
                     repository)
        return 0

    program = PROGRAMS[program_name]
    if step:
        program = [x for x in PROGRAMS['full'] if x.__name__ in step]

    if pw_presubmit.run_presubmit(program,
                                  repository=repository,
                                  output_directory=output_directory,
                                  **presubmit_args):
        return 0

    return 1


if __name__ == '__main__':
    # By default, display log messages like a simple print statement.
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    sys.exit(main(**vars(argument_parser().parse_args())))
