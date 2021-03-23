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
import shutil
import sys
from typing import Sequence, IO, Tuple, Optional

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

import pw_package.pigweed_packages

from pw_presubmit import build, cli, format_code, git_repo
from pw_presubmit import call, filter_paths, plural, PresubmitContext
from pw_presubmit import PresubmitFailure, Programs
from pw_presubmit.install_hook import install_hook

_LOG = logging.getLogger(__name__)

pw_package.pigweed_packages.initialize()

# Trigger builds if files with these extensions change.
_BUILD_EXTENSIONS = ('.py', '.rst', '.gn', '.gni',
                     *format_code.C_FORMAT.extensions)


def _at_all_optimization_levels(target):
    levels = ('debug', 'size_optimized', 'speed_optimized')

    # Skip optimized host GCC builds for now, since GCC sometimes emits spurious
    # warnings.
    #
    #   -02: GCC 9.3 emits spurious maybe-uninitialized warnings
    #   -0s: GCC 8.1 (Mingw-w64) emits a spurious nonnull warning
    #
    # TODO(pwbug/255): Enable optimized GCC builds when this is fixed.
    if target == 'host_gcc':
        levels = ('debug', )

    for level in levels:
        yield f'{target}_{level}'


#
# Build presubmit checks
#
def gn_clang_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('host_clang'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_gcc_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('host_gcc'))


_HOST_COMPILER = 'gcc' if sys.platform == 'win32' else 'clang'


def gn_host_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir,
                *_at_all_optimization_levels(f'host_{_HOST_COMPILER}'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_quick_build_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)

    # TODO(pwbug/255): Switch to optimized GCC builds when this is fixed.
    # See comment in _at_all_optimization_levels() above for details.
    optimization_level = 'size_optimized'
    if _HOST_COMPILER == 'gcc':
        optimization_level = 'debug'

    build.ninja(ctx.output_dir, f'host_{_HOST_COMPILER}_{optimization_level}',
                'stm32f429i_size_optimized', 'python.tests', 'python.lint')


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_full_build_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('stm32f429i'),
                *_at_all_optimization_levels(f'host_{_HOST_COMPILER}'),
                'python.tests', 'python.lint', 'docs')


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_full_qemu_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('qemu_gcc'),
                *_at_all_optimization_levels('qemu_clang'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_arm_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('stm32f429i'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_nanopb_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'nanopb')
    build.gn_gen(ctx.root,
                 ctx.output_dir,
                 dir_pw_third_party_nanopb='"{}"'.format(ctx.package_root /
                                                         'nanopb'))
    build.ninja(
        ctx.output_dir,
        *_at_all_optimization_levels('stm32f429i'),
        *_at_all_optimization_levels('host_clang'),
    )


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_teensy_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'teensy')
    build.gn_gen(ctx.root,
                 ctx.output_dir,
                 pw_arduino_build_CORE_PATH='"{}"'.format(str(
                     ctx.package_root)),
                 pw_arduino_build_CORE_NAME='teensy',
                 pw_arduino_build_PACKAGE_NAME='teensy/avr',
                 pw_arduino_build_BOARD='teensy40')
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('arduino'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_qemu_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('qemu_gcc'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_qemu_clang_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('qemu_clang'))


def gn_docs_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, 'docs')


def gn_host_tools(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, pw_build_HOST_TOOLS=True)
    build.ninja(ctx.output_dir, 'host')


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def oss_fuzz_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, pw_toolchain_OSS_FUZZ_ENABLED=True)
    build.ninja(ctx.output_dir, "fuzzers")


@filter_paths(endswith='.py')
def python_checks(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(
        ctx.output_dir,
        ':python.lint',
        ':python.tests',
        ':target_support_packages.lint',
        ':target_support_packages.tests',
    )


def _run_cmake(ctx: PresubmitContext) -> None:
    build.install_package(ctx.package_root, 'nanopb')

    toolchain = ctx.root / 'pw_toolchain' / 'host_clang' / 'toolchain.cmake'
    build.cmake(ctx.root,
                ctx.output_dir,
                f'-DCMAKE_TOOLCHAIN_FILE={toolchain}',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=1',
                f'-Ddir_pw_third_party_nanopb={ctx.package_root / "nanopb"}',
                '-Dpw_third_party_nanopb_ADD_SUBDIRECTORY=ON',
                env=build.env_with_clang_vars())


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.cmake',
                        'CMakeLists.txt'))
def cmake_tests(ctx: PresubmitContext):
    _run_cmake(ctx)
    build.ninja(ctx.output_dir, 'pw_apps', 'pw_run_tests.modules')


# TODO: Slowly add modules here that work with bazel until all
# modules are added. Then replace with //...
_MODULES_THAT_WORK_WITH_BAZEL = [
    '//pw_assert_basic/...',
    '//pw_base64/...',
    '//pw_build/...',
    '//pw_chrono_stl/...',
    '//pw_containers/...',
    '//pw_cpu_exception/...',
    '//pw_docgen/...',
    '//pw_doctor/...',
    '//pw_i2c/...',
    '//pw_log/...',
    '//pw_log_basic/...',
    '//pw_polyfill/...',
    '//pw_preprocessor/...',
    '//pw_protobuf_compiler/...',
    '//pw_span/...',
    '//pw_status/...',
    '//pw_sys_io/...',
    '//pw_sys_io_baremetal_lm3s6965evb/...',
    '//pw_sys_io_stdio/...',
    '//pw_thread_stl/...',
    '//pw_toolchain/...',
    '//pw_varint/...',
    '//pw_web_ui/...',
]


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.bzl', 'BUILD'))
def bazel_test(ctx: PresubmitContext):
    """Runs bazel test on each bazel compatible module"""

    try:
        call('bazel',
             'test',
             *_MODULES_THAT_WORK_WITH_BAZEL,
             '--verbose_failures',
             '--verbose_explanations',
             '--worker_verbose',
             '--test_output=errors',
             cwd=ctx.root,
             env=build.env_with_clang_vars())
    except:
        _LOG.info('If the Bazel build inexplicably fails while the '
                  'other builds are passing, try deleting the Bazel cache:\n'
                  '    rm -rf ~/.cache/bazel')
        raise


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.bzl', 'BUILD'))
def bazel_build(ctx: PresubmitContext):
    """Runs Bazel build on each Bazel compatible module"""
    try:
        call('bazel',
             'build',
             *_MODULES_THAT_WORK_WITH_BAZEL,
             '--verbose_failures',
             '--verbose_explanations',
             '--worker_verbose',
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


# The first line must be regex because of the '20\d\d' date
COPYRIGHT_FIRST_LINE = r'Copyright 20\d\d The Pigweed Authors'
COPYRIGHT_COMMENTS = r'(#|//| \*|REM|::)'
COPYRIGHT_BLOCK_COMMENTS = (
    # HTML comments
    (r'<!--', r'-->'), )

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
""".splitlines())

_EXCLUDE_FROM_COPYRIGHT_NOTICE: Sequence[str] = (
    # Configuration
    r'^(?:.+/)?\..+$',
    r'\bPW_PLUGINS$',
    # Metadata
    r'^docker/tag$',
    r'\bAUTHORS$',
    r'\bLICENSE$',
    r'\bOWNERS$',
    r'\brequirements.txt$',
    r'\bgo.(mod|sum)$',
    r'\bpackage.json$',
    r'\byarn.lock$',
    # Data files
    r'\.elf$',
    r'\.gif$',
    r'\.jpg$',
    r'\.json$',
    r'\.png$',
    # Documentation
    r'\.md$',
    r'\.rst$',
    # Generated protobuf files
    r'\.pb\.h$',
    r'\.pb\.c$',
    r'\_pb2.pyi?$',
    # Diff/Patch files
    r'\.diff$',
    r'\.patch$',
)


def match_block_comment_start(line: str) -> Optional[str]:
    """Matches the start of a block comment and returns the end."""
    for block_comment in COPYRIGHT_BLOCK_COMMENTS:
        if re.match(block_comment[0], line):
            # Return the end of the block comment
            return block_comment[1]
    return None


def copyright_read_first_line(
        file: IO) -> Tuple[Optional[str], Optional[str], Optional[str]]:
    """Reads the file until it reads a valid first copyright line.

    Returns (comment, block_comment, line). comment and block_comment are
    mutually exclusive and refer to the comment character sequence and whether
    they form a block comment or a line comment. line is the first line of
    the copyright, and is used for error reporting.
    """
    line = file.readline()
    first_line_matcher = re.compile(COPYRIGHT_COMMENTS + ' ' +
                                    COPYRIGHT_FIRST_LINE)
    while line:
        end_block_comment = match_block_comment_start(line)
        if end_block_comment:
            next_line = file.readline()
            copyright_line = re.match(COPYRIGHT_FIRST_LINE, next_line)
            if not copyright_line:
                return (None, None, line)
            return (None, end_block_comment, line)

        first_line = first_line_matcher.match(line)
        if first_line:
            return (first_line.group(1), None, line)

        if (line.strip()
                and not line.startswith(COPYRIGHT_FIRST_LINE_EXCEPTIONS)):
            return (None, None, line)

        line = file.readline()
    return (None, None, None)


@filter_paths(exclude=_EXCLUDE_FROM_COPYRIGHT_NOTICE)
def copyright_notice(ctx: PresubmitContext):
    """Checks that the Pigweed copyright notice is present."""
    errors = []

    for path in ctx.paths:

        if path.stat().st_size == 0:
            continue  # Skip empty files

        with path.open() as file:
            (comment, end_block_comment,
             line) = copyright_read_first_line(file)

            if not line:
                _LOG.warning('%s: invalid first line', path)
                errors.append(path)
                continue

            if not (comment or end_block_comment):
                _LOG.warning('%s: invalid first line %r', path, line)
                errors.append(path)
                continue

            if end_block_comment:
                expected_lines = COPYRIGHT_LINES + (end_block_comment, )
            else:
                expected_lines = COPYRIGHT_LINES

            for expected, actual in zip(expected_lines, file):
                if end_block_comment:
                    expected_line = expected + '\n'
                elif comment:
                    expected_line = (comment + ' ' + expected).rstrip() + '\n'

                if expected_line != actual:
                    _LOG.warning('  bad line: %r', actual)
                    _LOG.warning('  expected: %r', expected_line)
                    errors.append(path)
                    break

    if errors:
        _LOG.warning('%s with a missing or incorrect copyright notice:\n%s',
                     plural(errors, 'file'), '\n'.join(str(e) for e in errors))
        raise PresubmitFailure


_BAZEL_SOURCES_IN_BUILD = tuple(format_code.C_FORMAT.extensions)
_GN_SOURCES_IN_BUILD = '.rst', '.py', *_BAZEL_SOURCES_IN_BUILD


@filter_paths(endswith=(*_GN_SOURCES_IN_BUILD, 'BUILD', '.bzl', '.gn', '.gni'))
def source_is_in_build_files(ctx: PresubmitContext):
    """Checks that source files are in the GN and Bazel builds."""
    missing = build.check_builds_for_files(
        _BAZEL_SOURCES_IN_BUILD,
        _GN_SOURCES_IN_BUILD,
        ctx.paths,
        bazel_dirs=[ctx.root],
        gn_build_files=git_repo.list_files(pathspecs=['BUILD.gn', '*BUILD.gn'],
                                           repo_path=ctx.root))

    if missing:
        _LOG.warning(
            'All source files must appear in BUILD and BUILD.gn files')
        raise PresubmitFailure

    _run_cmake(ctx)
    cmake_missing = build.check_compile_commands_for_files(
        ctx.output_dir / 'compile_commands.json',
        (f for f in ctx.paths if f.suffix in ('.c', '.cc')))
    if cmake_missing:
        _LOG.warning('The CMake build is missing %d files', len(cmake_missing))
        _LOG.warning('Files missing from CMake:\n%s',
                     '\n'.join(str(f) for f in cmake_missing))
        # TODO(hepler): Many files are missing from the CMake build. Make this
        #     check an error when the missing files are fixed.
        # raise PresubmitFailure


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


def commit_message_format(_: PresubmitContext):
    """Checks that the top commit's message is correctly formatted."""
    lines = git_repo.commit_message().splitlines()

    # Show limits and current commit message in log.
    _LOG.debug('%-25s%+25s%+22s', 'Line limits', '72|', '72|')
    for line in lines:
        _LOG.debug(line)

    # Ignore Gerrit-generated reverts.
    if ('Revert' in lines[0]
            and 'This reverts commit ' in git_repo.commit_message()
            and 'Reason for revert: ' in git_repo.commit_message()):
        _LOG.warning('Ignoring apparent Gerrit-generated revert')
        return

    if not lines:
        _LOG.error('The commit message is too short!')
        raise PresubmitFailure

    errors = 0

    if len(lines[0]) > 72:
        _LOG.warning("The commit message's first line must be no longer than "
                     '72 characters.')
        _LOG.warning('The first line is %d characters:\n  %s', len(lines[0]),
                     lines[0])
        errors += 1

    if lines[0].endswith('.'):
        _LOG.warning(
            "The commit message's first line must not end with a period:\n %s",
            lines[0])
        errors += 1

    if len(lines) > 1 and lines[1]:
        _LOG.warning("The commit message's second line must be blank.")
        _LOG.warning('The second line has %d characters:\n  %s', len(lines[1]),
                     lines[1])
        errors += 1

    # Check that the lines are 72 characters or less, but skip any lines that
    # might possibly have a URL, path, or metadata in them. Also skip any lines
    # with non-ASCII characters.
    for i, line in enumerate(lines[2:], 3):
        if any(c in line for c in ':/>') or not line.isascii():
            continue

        if len(line) > 72:
            _LOG.warning(
                'Commit message lines must be no longer than 72 characters.')
            _LOG.warning('Line %d has %d characters:\n  %s', i, len(line),
                         line)
            errors += 1

    if errors:
        _LOG.error('Found %s in the commit message', plural(errors, 'error'))
        raise PresubmitFailure


def static_analysis(ctx: PresubmitContext):
    """Check that files pass static analyzer checks."""
    build.gn_gen(ctx.root, ctx.output_dir,
                 '--export-compile-commands=host_clang_debug')
    build.ninja(ctx.output_dir, 'host_clang_debug')

    compile_commands = ctx.output_dir.joinpath('compile_commands.json')
    analyzer_output = ctx.output_dir.joinpath('analyze-build-output')

    if analyzer_output.exists():
        shutil.rmtree(analyzer_output)

    call('analyze-build',
         '--cdb',
         compile_commands,
         '--exclude',
         'third_party',
         '--output',
         analyzer_output,
         cwd=ctx.root,
         env=build.env_with_clang_vars())

    # Search for reports under output directory.
    reports = list(analyzer_output.glob('*/report*'))
    if len(reports) != 0:
        archive = shutil.make_archive(str(analyzer_output), 'zip',
                                      reports[0].parent)
        _LOG.error('Static analyzer found errors: %s', archive)
        _LOG.error('To view report, open: %s',
                   Path(reports[0]).parent.joinpath('index.html'))
        raise PresubmitFailure


def renode_check(ctx: PresubmitContext):
    """Placeholder for future check."""
    _LOG.info('%s %s', ctx.root, ctx.output_dir)


#
# Presubmit check programs
#

OTHER_CHECKS = (
    # TODO(pwbug/45): Remove clang-tidy from OTHER_CHECKS when it passes.
    clang_tidy,
    # Build that attempts to duplicate the build OSS-Fuzz does. Currently
    # failing.
    oss_fuzz_build,
    bazel_test,
    cmake_tests,
    gn_nanopb_build,
    gn_full_build_check,
    gn_full_qemu_check,
    gn_clang_build,
    gn_gcc_build,
    renode_check,
    static_analysis,
)

LINTFORMAT = (
    commit_message_format,
    copyright_notice,
    format_code.presubmit_checks(),
    pw_presubmit.pragma_once,
    source_is_in_build_files,
)

QUICK = (
    LINTFORMAT,
    bazel_test,
    gn_quick_build_check,
    # TODO(pwbug/141): Re-enable CMake and Bazel for Mac after we have fixed the
    # the clang issues. The problem is that all clang++ invocations need the
    # two extra flags: "-nostdc++" and "${clang_prefix}/../lib/libc++.a".
    cmake_tests if sys.platform != 'darwin' else (),
)

FULL = (
    LINTFORMAT,
    gn_host_build,
    gn_arm_build,
    gn_docs_build,
    gn_host_tools,
    bazel_build,
    bazel_test,
    # On Mac OS, system 'gcc' is a symlink to 'clang' by default, so skip GCC
    # host builds on Mac for now. Skip it on Windows too, since gn_host_build
    # already uses 'gcc' on Windows.
    gn_gcc_build if sys.platform not in ('darwin', 'win32') else (),
    # Windows doesn't support QEMU yet.
    gn_qemu_build if sys.platform != 'win32' else (),
    gn_qemu_clang_build if sys.platform != 'win32' else (),
    source_is_in_build_files,
    python_checks,
    build_env_setup,
    # Skip gn_teensy_build if running on Windows. The Teensycore installer is
    # an exe that requires an admin role.
    gn_teensy_build if sys.platform in ['linux', 'darwin'] else (),
)

PROGRAMS = Programs(
    full=FULL,
    lintformat=LINTFORMAT,
    other_checks=OTHER_CHECKS,
    quick=QUICK,
)


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
