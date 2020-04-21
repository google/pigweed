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

from pw_presubmit import build, environment, format_code, python_checks
from pw_presubmit.install_hook import install_hook
from pw_presubmit import call, filter_paths, PresubmitContext, PresubmitFailure

_LOG = logging.getLogger(__name__)


#
# Initialization
#
def init_cipd(ctx: PresubmitContext):
    environment.init_cipd(ctx.repo_root, ctx.output_dir)


def init_virtualenv(ctx: PresubmitContext):
    environment.init_cipd(ctx.repo_root, ctx.output_dir)


#
# GN presubmit checks
#
_CLANG_GEN_ARGS = build.gn_args(
    pw_target_config='"//targets/host/target_config.gni"',
    pw_target_toolchain='"//pw_toolchain:host_clang_os"')

_DOCS_GEN_ARGS = build.gn_args(
    pw_target_config='"//targets/docs/target_config.gni"')


def gn_clang_build(ctx: PresubmitContext):
    build.gn_gen(ctx.repo_root, ctx.output_dir, _CLANG_GEN_ARGS)
    build.ninja(ctx.output_dir)


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_gcc_build(ctx: PresubmitContext):
    build.gn_gen(
        ctx.repo_root, ctx.output_dir,
        build.gn_args(pw_target_config='"//targets/host/target_config.gni"',
                      pw_target_toolchain='"//pw_toolchain:host_gcc_os"'))
    build.ninja(ctx.output_dir)


_ARM_GEN_ARGS = build.gn_args(
    pw_target_config='"//targets/stm32f429i-disc1/target_config.gni"')


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_arm_build(ctx: PresubmitContext):
    build.gn_gen(ctx.repo_root, ctx.output_dir, _ARM_GEN_ARGS)
    build.ninja(ctx.output_dir)


def gn_docs_build(ctx: PresubmitContext):
    build.gn_gen(ctx.repo_root, ctx.output_dir, _DOCS_GEN_ARGS)
    build.ninja(ctx.output_dir, 'docs:docs')


_QEMU_GEN_ARGS = build.gn_args(
    pw_target_config='"//targets/lm3s6965evb-qemu/target_config.gni"')

GN: Tuple[Callable, ...] = (
    gn_clang_build,
    gn_arm_build,
    gn_docs_build,
)

# On Mac OS, 'gcc' is a symlink to 'clang', so skip GCC host builds on Mac.
if sys.platform != 'darwin':
    GN = GN + (gn_gcc_build, )


def gn_host_tools(ctx: PresubmitContext):
    build.gn_gen(ctx.repo_root,
                 ctx.output_dir,
                 pw_target_config='"//targets/host/target_config.gni"',
                 pw_target_toolchain='"//pw_toolchain:host_clang_os"',
                 pw_build_host_tools='true')
    build.ninja(ctx.output_dir, 'host_tools')


#
# C++ presubmit checks
#
# TODO(pwbug/45) Probably want additional checks.
_CLANG_TIDY_CHECKS = ('modernize-use-override', )


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def clang_tidy(ctx: PresubmitContext):
    build.gn_gen(ctx.repo_root, ctx.output_dir, '--export-compile-commands',
                 _CLANG_GEN_ARGS)
    build.ninja(ctx.output_dir)
    build.ninja(ctx.output_dir, '-t', 'compdb', 'objcxx', 'cxx')

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
        f'-p={ctx.output_dir}',
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
@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.cmake',
                        'CMakeLists.txt'))
def cmake_tests(ctx: PresubmitContext):
    build.cmake(ctx.repo_root, ctx.output_dir, env=build.env_with_clang_vars())
    build.ninja(ctx.output_dir, 'pw_run_tests.modules')


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
             ctx.output_dir.joinpath('bazel-'),
             cwd=ctx.repo_root,
             env=build.env_with_clang_vars())
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

COPYRIGHT_FIRST_LINE_EXCEPTIONS = (
    '#!',
    '/*',
    '@echo off',
    '# -*-',
    ':',
)

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
    r'PW_PLUGINS',
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
    """Checks that the Pigweed copyright notice is present."""

    errors = []

    for path in ctx.paths:
        _LOG.debug('Checking %s', path)
        with open(path) as file:
            line = file.readline()
            first_line = None
            while line:
                first_line = COPYRIGHT_FIRST_LINE.match(line)
                if first_line:
                    break

                if (line.strip() and
                        not line.startswith(COPYRIGHT_FIRST_LINE_EXCEPTIONS)):
                    break

                line = file.readline()

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
_SOURCES_IN_BUILD = '.rst', *format_code.C_FORMAT.extensions


@filter_paths(endswith=(*_SOURCES_IN_BUILD, 'BUILD', '.bzl', '.gn', '.gni'))
def source_is_in_build_files(ctx: PresubmitContext):
    """Checks that source files are in the GN and Bazel builds."""
    gn_gens_to_run = (
        (ctx.output_dir.joinpath('arm'), _ARM_GEN_ARGS),
        (ctx.output_dir.joinpath('clang'), _CLANG_GEN_ARGS),
        (ctx.output_dir.joinpath('docs'), _DOCS_GEN_ARGS),
        (ctx.output_dir.joinpath('qemu'), _QEMU_GEN_ARGS),
    )

    for directory, args in gn_gens_to_run:
        build.gn_gen(ctx.repo_root, directory, args)

    missing = build.check_builds_for_files(_SOURCES_IN_BUILD,
                                           ctx.paths,
                                           bazel_dirs=[ctx.repo_root],
                                           gn_dirs=[
                                               (ctx.repo_root, path)
                                               for path, _ in gn_gens_to_run
                                           ])

    if missing:
        _LOG.warning(
            'All source files must appear in BUILD and BUILD.gn files')
        raise PresubmitFailure


GENERAL: Tuple[Callable, ...] = (source_is_in_build_files, )


def build_env_setup(ctx: PresubmitContext):
    if 'PW_CARGO_SETUP' not in os.environ:
        _LOG.warning(
            'Skipping build_env_setup since PW_CARGO_SETUP is not set')
        return

    tmpl = ctx.repo_root.joinpath('pw_env_setup', 'py', 'pyoxidizer.bzl.tmpl')
    out = ctx.output_dir.joinpath('pyoxidizer.bzl')

    with open(tmpl, 'r') as ins:
        cfg = ins.read().replace('${PW_ROOT}', str(ctx.repo_root))
        with open(out, 'w') as outs:
            outs.write(cfg)

    call('pyoxidizer', 'build', cwd=ctx.output_dir)


BUILD_ENV_SETUP = (build_env_setup, )


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
    init_cipd,
    init_virtualenv,
    *CODE_FORMAT,
    *GENERAL,
    *CC,
    gn_clang_build,
    gn_arm_build,
    *BAZEL,
    *python_checks.ALL,
)

FULL_PRESUBMIT: Tuple[Callable, ...] = sum([
    (init_cipd, init_virtualenv),
    CODE_FORMAT,
    CC,
    GN,
    python_checks.ALL,
    CMAKE,
    BAZEL,
    GENERAL,
    BUILD_ENV_SETUP,
], ())

PROGRAMS: Dict[str, Tuple] = {
    'broken': BROKEN,
    'full': FULL_PRESUBMIT,
    'quick': QUICK_PRESUBMIT,
}

ALL_STEPS = {c.__name__: c for c in itertools.chain(*PROGRAMS.values())}


def parse_args() -> argparse.Namespace:
    """Creates an argument parser and parses arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-o',
        '--output-directory',
        type=Path,
        help='Output directory (default: <repo root>/.presubmit)',
    )

    exclusive = parser.add_mutually_exclusive_group()
    exclusive.add_argument(
        '--clear',
        '--clean',
        action='store_true',
        help='Delete the presubmit output directory and exit.',
    )
    exclusive.add_argument(
        '--clear-py',
        action='store_true',
        help='Delete the Python virtualenv and exit.',
    )
    exclusive.add_argument(
        '--install',
        action='store_true',
        help='Install the presubmit as a Git pre-push hook and exit.')
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

    return parser.parse_args()


def run(
    program_name: str,
    clear: bool,
    clear_py: bool,
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
    _LOG.debug('Using environment at %s', output_directory)

    if clear or clear_py:
        _LOG.info('Clearing presubmit%s environment',
                  '' if clear else ' Python')

        if clear and output_directory.exists():
            shutil.rmtree(output_directory)
            _LOG.info('Deleted %s', output_directory)

        init_venv = output_directory.joinpath('init_virtualenv')
        if clear_py and init_venv.exists():
            shutil.rmtree(init_venv)
            _LOG.info('Deleted %s', init_venv)

        return 0

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


def main() -> int:
    """Run the presubmit for the Pigweed repository."""
    return run(**vars(parse_args()))


if __name__ == '__main__':
    try:
        # If pw_cli is available, use it to initialize logs.
        from pw_cli import log

        log.install(logging.INFO)
    except ImportError:
        # If pw_cli isn't available, display log messages like a simple print.
        logging.basicConfig(format='%(message)s', level=logging.INFO)

    sys.exit(main())
