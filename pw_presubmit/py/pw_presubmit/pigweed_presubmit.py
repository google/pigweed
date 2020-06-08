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
import logging
import os
from pathlib import Path
import re
import sys
from typing import Sequence

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_presubmit import build, cli, environment, format_code, git_repo
from pw_presubmit import python_checks
from pw_presubmit import call, filter_paths, PresubmitContext, PresubmitFailure
from pw_presubmit import Programs
from pw_presubmit.install_hook import install_hook

_LOG = logging.getLogger(__name__)


#
# Initialization
#
def init_cipd(ctx: PresubmitContext):
    environment.init_cipd(ctx.root, ctx.output_dir)


def init_virtualenv(ctx: PresubmitContext):
    environment.init_cipd(ctx.root, ctx.output_dir)


#
# Build presubmit checks
#
def gn_clang_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, "host_clang")


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_gcc_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, "host_gcc")


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_arm_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, "stm32f429i")


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def gn_qemu_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, "qemu")


def gn_docs_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, 'docs')


def gn_host_tools(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, pw_build_HOST_TOOLS='true')
    build.ninja(ctx.output_dir)


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def oss_fuzz_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root,
                 ctx.output_dir,
                 pw_toolchain_OSS_FUZZ_ENABLED='true',
                 pw_toolchain_SANITIZER='"address"')
    build.ninja(ctx.output_dir, "host_clang")


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.cmake',
                        'CMakeLists.txt'))
def cmake_tests(ctx: PresubmitContext):
    build.cmake(ctx.root, ctx.output_dir, env=build.env_with_clang_vars())
    build.ninja(ctx.output_dir, 'pw_run_tests.modules')


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
             cwd=ctx.root,
             env=build.env_with_clang_vars())
    except:
        _LOG.info('If the Bazel build inexplicably fails while the '
                  'other builds are passing, try deleting the Bazel cache:\n'
                  '    rm -rf ~/.cache/bazel')
        raise


#
# General presubmit checks
#

# TODO(pwbug/45) Probably want additional checks.
_CLANG_TIDY_CHECKS = ('modernize-use-override', )


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def clang_tidy(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, '--export-compile-commands')
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
    r'^(?:.+/)?\..+$',
    r'\bAUTHORS$',
    r'\bLICENSE$',
    r'\bOWNERS$',
    r'\bPW_PLUGINS$',
    r'\.elf$',
    r'\.gif$',
    r'\.jpg$',
    r'\.json$',
    r'\.md$',
    r'\.png$',
    r'\.rst$',
    r'\brequirements.txt$',
    r'\bgo.(mod|sum)$',
    r'\bpackage.json$',
    r'\byarn.lock$',
    r'\bpw_web_ui/types/serial.d.ts$',
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


_SOURCES_IN_BUILD = '.rst', *format_code.C_FORMAT.extensions


@filter_paths(endswith=(*_SOURCES_IN_BUILD, 'BUILD', '.bzl', '.gn', '.gni'))
def source_is_in_build_files(ctx: PresubmitContext):
    """Checks that source files are in the GN and Bazel builds."""
    missing = build.check_builds_for_files(
        _SOURCES_IN_BUILD,
        ctx.paths,
        bazel_dirs=[ctx.root],
        gn_build_files=git_repo.list_files(
            pathspecs=['BUILD.gn', '*BUILD.gn']))

    if missing:
        _LOG.warning(
            'All source files must appear in BUILD and BUILD.gn files')
        raise PresubmitFailure


def build_env_setup(ctx: PresubmitContext):
    if 'PW_CARGO_SETUP' not in os.environ:
        _LOG.warning(
            'Skipping build_env_setup since PW_CARGO_SETUP is not set')
        return

    tmpl = ctx.root.joinpath('pw_env_setup', 'py', 'pyoxidizer.bzl.tmpl')
    out = ctx.output_dir.joinpath('pyoxidizer.bzl')

    with open(tmpl, 'r') as ins:
        cfg = ins.read().replace('${PW_ROOT}', str(ctx.root))
        with open(out, 'w') as outs:
            outs.write(cfg)

    call('pyoxidizer', 'build', cwd=ctx.output_dir)


#
# Presubmit check programs
#

BROKEN = (
    # TODO(pwbug/45): Remove clang-tidy from BROKEN when it passes.
    clang_tidy,
    # Host tools are not broken but take long on slow internet connections.
    # They're still run in CQ, but not in 'pw presubmit'.
    gn_host_tools,
    # QEMU build. Currently doesn't have test runners, and can't build one
    # of the fuzzing targets.
    gn_qemu_build,
    # Build that attempts to duplicate the build OSS-Fuzz does. Currently
    # failing.
    oss_fuzz_build,
)

QUICK = (
    init_cipd,
    init_virtualenv,
    copyright_notice,
    format_code.presubmit_checks(),
    pw_presubmit.pragma_once,
    gn_clang_build,
    gn_arm_build,
    source_is_in_build_files,
    python_checks.all_checks(),
)

FULL = (
    init_cipd,
    init_virtualenv,
    copyright_notice,
    format_code.presubmit_checks(),
    pw_presubmit.pragma_once,
    gn_clang_build,
    gn_arm_build,
    gn_docs_build,
    # On Mac OS, system 'gcc' is a symlink to 'clang' by default, so skip GCC
    # host builds on Mac for now.
    gn_gcc_build if sys.platform != 'darwin' else (),
    # TODO(pwbug/141): Re-enable CMake and Bazel for Mac after we have fixed the
    # the clang issues. The problem is that all clang++ invocations need the
    # two extra flags: "-nostdc++" and "${clang_prefix}../lib/libc++.a".
    cmake_tests if sys.platform != 'darwin' else (),
    bazel_test if sys.platform != 'darwin' else (),
    source_is_in_build_files,
    python_checks.all_checks(),
    build_env_setup,
)

PROGRAMS = Programs(broken=BROKEN, quick=QUICK, full=FULL)


def parse_args() -> argparse.Namespace:
    """Creates an argument parser and parses arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    cli.add_arguments(parser, PROGRAMS, 'quick')
    parser.add_argument(
        '--install',
        action='store_true',
        help='Install the presubmit as a Git pre-push hook and exit.')

    return parser.parse_args()


def run(install: bool, **presubmit_args) -> int:
    """Entry point for presubmit."""

    if install:
        install_hook(__file__, 'pre-push',
                     ['--base', 'origin/master..HEAD', '--program', 'quick'],
                     Path.cwd())
        return 0

    return cli.run(**presubmit_args)


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
