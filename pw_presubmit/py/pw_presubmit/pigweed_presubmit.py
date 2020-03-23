#!/usr/bin/env python3

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
"""Runs the local presubmit checks for the Pigweed repository."""

import argparse
import itertools
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Callable, Dict, Sequence, Tuple

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_presubmit import python_checks
from pw_presubmit import format_code, PresubmitContext
from pw_presubmit.install_hook import install_hook
from pw_presubmit import call, filter_paths, log_run, plural, PresubmitFailure

_LOG = logging.getLogger(__name__)


#
# Initialization
#
def init_cipd(ctx: PresubmitContext):
    # TODO(mohrr) invoke by importing rather than by subprocess.
    call(
        sys.executable,
        ctx.repository_root.joinpath('pw_env_setup', 'py', 'pw_env_setup',
                                     'cipd_setup', 'update.py'),
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
    virtualenv_source = ctx.repository_root.joinpath('pw_env_setup', 'py',
                                                     'pw_env_setup',
                                                     'virtualenv_setup')

    # For speed, don't build the venv if it exists. Use --clean to recreate it.
    if not ctx.output_directory.joinpath('pyvenv.cfg').is_file():
        call(
            'python3',
            virtualenv_source,
            f'--venv_path={ctx.output_directory}',
            '--requirements={}'.format(
                virtualenv_source.joinpath('requirements.txt')),
        )

    os.environ['PATH'] = os.pathsep.join((
        str(ctx.output_directory.joinpath('bin')),
        os.environ['PATH'],
    ))


INIT: Tuple[Callable, ...] = (
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


def ninja(*args, ctx: PresubmitContext, **kwargs):
    call('ninja',
         '-C',
         ctx.output_directory,
         *args,
         cwd=ctx.repository_root,
         **kwargs)


_CLANG_GEN_ARGS = gn_args(
    pw_target_config='"//targets/host/target_config.gni"',
    pw_target_toolchain='"//pw_toolchain:host_clang_os"')

_DOCS_GEN_ARGS = gn_args(pw_target_config='"//targets/docs/target_config.gni"')


def gn_clang_build(ctx: PresubmitContext):
    gn_gen(_CLANG_GEN_ARGS, ctx=ctx)
    ninja(ctx=ctx)


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_gcc_build(ctx: PresubmitContext):
    gn_gen(gn_args(pw_target_config='"//targets/host/target_config.gni"',
                   pw_target_toolchain='"//pw_toolchain:host_gcc_os"'),
           ctx=ctx)
    ninja(ctx=ctx)


_ARM_GEN_ARGS = gn_args(
    pw_target_config='"//targets/stm32f429i-disc1/target_config.gni"')


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_arm_build(ctx: PresubmitContext):
    gn_gen(_ARM_GEN_ARGS, ctx=ctx)
    ninja(ctx=ctx)


def gn_docs_build(ctx: PresubmitContext):
    gn_gen(_DOCS_GEN_ARGS, ctx=ctx)
    ninja('docs:docs', ctx=ctx)


GN: Tuple[Callable, ...] = (
    gn_clang_build,
    gn_arm_build,
    gn_docs_build,
)

# On Mac OS, 'gcc' is a symlink to 'clang', so skip GCC host builds on Mac.
if sys.platform != 'darwin':
    GN = GN + (gn_gcc_build, )


def gn_host_tools(ctx: PresubmitContext):
    gn_gen(gn_args(pw_target_config='"//targets/host/target_config.gni"',
                   pw_target_toolchain='"//pw_toolchain:host_clang_os"',
                   pw_build_host_tools='true'),
           ctx=ctx)
    ninja('host_tools', ctx=ctx)


#
# C++ presubmit checks
#
# TODO(pwbug/45) Probably want additional checks.
_CLANG_TIDY_CHECKS = ('modernize-use-override', )


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def clang_tidy(ctx: PresubmitContext):
    gn_gen('--export-compile-commands', _CLANG_GEN_ARGS, ctx=ctx)
    ninja(ctx=ctx)
    ninja('-t', 'compdb', 'objcxx', 'cxx', ctx=ctx)

    run_clang_tidy = None
    for var in ('PW_PIGWEED_CIPD_INSTALL_DIR', 'PW_CIPD_INSTALL_DIR'):
        if var in os.environ:
            possibility = os.path.join(os.environ[var],
                                       'share/clang/run-clang-tidy.py')
            if os.path.isfile(possibility):
                run_clang_tidy = possibility
                break

    checks = ','.join(_CLANG_TIDY_CHECKS)
    call(
        run_clang_tidy,
        f'-p={ctx.output_directory}',
        f'-checks={checks}',
        # TODO(pwbug/45) not sure if this is needed.
        # f'-extra-arg-before=-warnings-as-errors={checks}',
        *ctx.paths)


CC: Tuple[Callable, ...] = (
    pw_presubmit.pragma_once,
    # TODO(pwbug/45): Enable clang-tidy when it passes.
    # clang_tidy,
)


#
# CMake presubmit checks
#
def _env_with_clang_cc_vars():
    env = os.environ.copy()
    env['CC'] = env['LD'] = env['AS'] = 'clang'
    env['CXX'] = 'clang++'
    return env


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.cmake',
                        'CMakeLists.txt'))
def cmake_tests(ctx: PresubmitContext):
    env = _env_with_clang_cc_vars()
    call('cmake',
         '-B', ctx.output_directory,
         '-S', ctx.repository_root,
         '-G', 'Ninja',
         env=env)  # yapf: disable
    ninja('pw_run_tests.modules', ctx=ctx)


CMAKE: Tuple[Callable, ...] = ()

# TODO(pwbug/141): Re-enable this after we have fixed the CMake toolchain issue
# on Mac. The problem is that all clang++ invocations need the two extra
# flags: "-nostdc++" and "${clang_prefix}../lib/libc++.a".
if sys.platform != 'darwin':
    CMAKE = (cmake_tests, )


#
# Bazel presubmit checks
#
@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.bzl', 'BUILD'))
def bazel_test(ctx: PresubmitContext):
    try:
        call('bazel',
             'test',
             '//...',
             '--verbose_failures',
             '--verbose_explanations',
             '--worker_verbose',
             '--symlink_prefix',
             ctx.output_directory.joinpath('bazel-'),
             cwd=ctx.repository_root)
    except:
        _LOG.info('If the Bazel build inexplicably fails while the '
                  'other builds are passing, try deleting the Bazel cache:\n'
                  '    rm -rf ~/.cache/bazel')
        raise


BAZEL: Tuple[Callable, ...] = ()

# TODO(pwbug/141): Re-enable this after we have fixed the Bazel toolchain issue
# on Mac. The problem is that all clang++ invocations need the two extra flags:
# "-nostdc++" and "${clang_prefix}../lib/libc++.a".
if sys.platform != 'darwin':
    BAZEL = (bazel_test, )

#
# Code format presubmit checks
#
COPYRIGHT_FIRST_LINE = re.compile(
    r'^(#|//| \*|REM|::) Copyright 20\d\d The Pigweed Authors$')

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
    r'OWNERS',
    r'.*\.elf',
    r'.*\.gif',
    r'.*\.jpg',
    r'.*\.json',
    r'.*\.md',
    r'.*\.png',
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
            while line and (line.startswith(
                ('#!', '/*', '@echo off', '# -*-')) or not line.strip()):
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


CODE_FORMAT: Tuple[Callable, ...] = (
    copyright_notice, *format_code.PRESUBMIT_CHECKS)  # yapf: disable

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

    docs_dir = ctx.output_directory.joinpath('docs')
    gn_gen(_DOCS_GEN_ARGS, ctx=ctx, path=docs_dir)
    build_gn.update(
        _get_paths_from_command('gn', 'desc', docs_dir, '*', ctx=ctx))

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


GENERAL: Tuple[Callable, ...] = (source_is_in_build_files, )

BROKEN: Tuple[Callable, ...] = (
    # TODO(pwbug/45): Remove clang-tidy from BROKEN when it passes.
    clang_tidy,
    # Host tools are not broken but take long on slow internet connections.
    # They're still run in CQ, but not in 'pw presubmit'.
    gn_host_tools,
)  # yapf: disable

#
# Presubmit check programs
#
QUICK_PRESUBMIT: Tuple[Callable, ...] = (
    *INIT,
    *CODE_FORMAT,
    *GENERAL,
    *CC,
    gn_clang_build,
    gn_arm_build,
    *python_checks.ALL,
)

FULL_PRESUBMIT: Tuple[Callable, ...] = (
    INIT + CODE_FORMAT + GENERAL + CC + GN +
    python_checks.ALL + CMAKE + BAZEL)  # yapf: disable

PROGRAMS: Dict[str, Tuple] = {
    'broken': BROKEN,
    'full': FULL_PRESUBMIT,
    'quick': QUICK_PRESUBMIT,
}

ALL_STEPS = {c.__name__: c for c in itertools.chain(*PROGRAMS.values())}


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
                           choices=[x for x in PROGRAMS if x != 'broken'],
                           default='full',
                           help='Which presubmit program to run')

    exclusive.add_argument(
        '--step',
        dest='steps',
        choices=sorted(ALL_STEPS),
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
        steps: Sequence[str],
        **presubmit_args,
) -> int:
    """Entry point for presubmit."""

    os.environ.setdefault('PW_ROOT',
                          str(pw_presubmit.git_repo_path(repo=repository)))

    if not output_directory:
        output_directory = pw_presubmit.git_repo_path('.presubmit',
                                                      repo=repository)
    environment = output_directory
    _LOG.debug('Using environment at %s', environment)

    if clean and environment.exists():
        shutil.rmtree(environment)
    elif clean_py and environment.joinpath('init_virtualenv').exists():
        shutil.rmtree(environment.joinpath('init_virtualenv'))

    if install:
        install_hook(__file__, 'pre-push',
                     ['--base', 'origin/master..HEAD', '--program', 'quick'],
                     repository)
        return 0

    program: Sequence = PROGRAMS[program_name]
    if steps:
        program = [ALL_STEPS[name] for name in steps]

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
