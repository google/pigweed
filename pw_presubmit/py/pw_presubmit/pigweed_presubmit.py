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
import json
import logging
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Sequence, IO, Tuple, Optional, Callable, List

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

import pw_package.pigweed_packages

from pw_presubmit import (
    build,
    cli,
    cpp_checks,
    format_code,
    git_repo,
    call,
    filter_paths,
    inclusive_language,
    plural,
    PresubmitContext,
    PresubmitFailure,
    Programs,
    python_checks,
)
from pw_presubmit.install_hook import install_hook

_LOG = logging.getLogger(__name__)

pw_package.pigweed_packages.initialize()

# Trigger builds if files with these extensions change.
_BUILD_EXTENSIONS = ('.py', '.rst', '.gn', '.gni',
                     *format_code.C_FORMAT.extensions)


def _at_all_optimization_levels(target):
    for level in ('debug', 'size_optimized', 'speed_optimized'):
        yield f'{target}_{level}'


#
# Build presubmit checks
#
def gn_clang_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root,
                 ctx.output_dir,
                 pw_RUN_INTEGRATION_TESTS=(sys.platform != 'win32'))
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
    """Checks the state of the GN build by running gn gen and gn check."""
    build.gn_gen(ctx.root, ctx.output_dir)


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_full_build_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('stm32f429i'),
                *_at_all_optimization_levels(f'host_{_HOST_COMPILER}'),
                'python.tests', 'python.lint', 'docs', 'fuzzers',
                'pw_env_setup:build_pigweed_python_source_tree')


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_full_qemu_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(
        ctx.output_dir,
        *_at_all_optimization_levels('qemu_gcc'),
        # TODO(pwbug/321) Re-enable clang.
        # *_at_all_optimization_levels('qemu_clang'),
    )


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_arm_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, *_at_all_optimization_levels('stm32f429i'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def stm32f429i(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, pw_use_test_server=True)
    with build.test_server('stm32f429i_disc1_test_server', ctx.output_dir):
        build.ninja(ctx.output_dir, *_at_all_optimization_levels('stm32f429i'))


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_boringssl_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'boringssl')
    build.gn_gen(ctx.root,
                 ctx.output_dir,
                 dir_pw_third_party_boringssl='"{}"'.format(ctx.package_root /
                                                            'boringssl'))
    build.ninja(
        ctx.output_dir,
        *_at_all_optimization_levels('stm32f429i'),
        *_at_all_optimization_levels('host_clang'),
    )


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
def gn_crypto_mbedtls_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'mbedtls')
    build.gn_gen(
        ctx.root,
        ctx.output_dir,
        dir_pw_third_party_mbedtls='"{}"'.format(ctx.package_root / 'mbedtls'),
        pw_crypto_SHA256_BACKEND='"{}"'.format(ctx.root /
                                               'pw_crypto:sha256_mbedtls'),
        pw_crypto_ECDSA_BACKEND='"{}"'.format(ctx.root /
                                              'pw_crypto:ecdsa_mbedtls'))
    build.ninja(ctx.output_dir)


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_crypto_boringssl_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'boringssl')
    build.gn_gen(
        ctx.root,
        ctx.output_dir,
        dir_pw_third_party_boringssl='"{}"'.format(ctx.package_root /
                                                   'boringssl'),
        pw_crypto_SHA256_BACKEND='"{}"'.format(ctx.root /
                                               'pw_crypto:sha256_boringssl'),
        pw_crypto_ECDSA_BACKEND='"{}"'.format(ctx.root /
                                              'pw_crypto:ecdsa_boringssl'),
    )
    build.ninja(ctx.output_dir)


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_crypto_micro_ecc_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'micro-ecc')
    build.gn_gen(
        ctx.root,
        ctx.output_dir,
        dir_pw_third_party_micro_ecc='"{}"'.format(ctx.package_root /
                                                   'micro-ecc'),
        pw_crypto_ECDSA_BACKEND='"{}"'.format(ctx.root /
                                              'pw_crypto:ecdsa_uecc'),
    )
    build.ninja(ctx.output_dir)


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
def gn_software_update_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'nanopb')
    build.install_package(ctx.package_root, 'protobuf')
    build.install_package(ctx.package_root, 'mbedtls')
    build.install_package(ctx.package_root, 'micro-ecc')
    build.gn_gen(
        ctx.root,
        ctx.output_dir,
        dir_pw_third_party_protobuf='"{}"'.format(ctx.package_root /
                                                  'protobuf'),
        dir_pw_third_party_nanopb='"{}"'.format(ctx.package_root / 'nanopb'),
        dir_pw_third_party_micro_ecc='"{}"'.format(ctx.package_root /
                                                   'micro-ecc'),
        pw_crypto_ECDSA_BACKEND='"{}"'.format(ctx.root /
                                              'pw_crypto:ecdsa_uecc'),
        dir_pw_third_party_mbedtls='"{}"'.format(ctx.package_root / 'mbedtls'),
        pw_crypto_SHA256_BACKEND='"{}"'.format(ctx.root /
                                               'pw_crypto:sha256_mbedtls'))
    build.ninja(
        ctx.output_dir,
        *_at_all_optimization_levels('host_clang'),
    )


@filter_paths(endswith=_BUILD_EXTENSIONS)
def gn_pw_system_demo_build(ctx: PresubmitContext):
    build.install_package(ctx.package_root, 'freertos')
    build.install_package(ctx.package_root, 'nanopb')
    build.install_package(ctx.package_root, 'stm32cube_f4')
    build.gn_gen(
        ctx.root,
        ctx.output_dir,
        dir_pw_third_party_freertos='"{}"'.format(ctx.package_root /
                                                  'freertos'),
        dir_pw_third_party_nanopb='"{}"'.format(ctx.package_root / 'nanopb'),
        dir_pw_third_party_stm32cube_f4='"{}"'.format(ctx.package_root /
                                                      'stm32cube_f4'),
    )
    build.ninja(ctx.output_dir, 'pw_system_demo')


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
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, 'host_tools')


@filter_paths(endswith=format_code.C_FORMAT.extensions)
def oss_fuzz_build(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, pw_toolchain_OSS_FUZZ_ENABLED=True)
    build.ninja(ctx.output_dir, "fuzzers")


def _run_cmake(ctx: PresubmitContext, toolchain='host_clang') -> None:
    build.install_package(ctx.package_root, 'nanopb')

    env = None
    if 'clang' in toolchain:
        env = build.env_with_clang_vars()

    toolchain_path = ctx.root / 'pw_toolchain' / toolchain / 'toolchain.cmake'
    build.cmake(ctx.root,
                ctx.output_dir,
                f'-DCMAKE_TOOLCHAIN_FILE={toolchain_path}',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=1',
                f'-Ddir_pw_third_party_nanopb={ctx.package_root / "nanopb"}',
                '-Dpw_third_party_nanopb_ADD_SUBDIRECTORY=ON',
                env=env)


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.cmake',
                        'CMakeLists.txt'))
def cmake_clang(ctx: PresubmitContext):
    _run_cmake(ctx, toolchain='host_clang')
    build.ninja(ctx.output_dir, 'pw_apps', 'pw_run_tests.modules')


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.cmake',
                        'CMakeLists.txt'))
def cmake_gcc(ctx: PresubmitContext):
    _run_cmake(ctx, toolchain='host_gcc')
    build.ninja(ctx.output_dir, 'pw_apps', 'pw_run_tests.modules')


# TODO(pwbug/180): Slowly add modules here that work with bazel until all
# modules are added. Then replace with //...
_MODULES_THAT_BUILD_WITH_BAZEL = [
    '//pw_allocator/...',
    '//pw_analog/...',
    '//pw_assert/...',
    '//pw_assert_basic/...',
    '//pw_assert_log/...',
    '//pw_base64/...',
    '//pw_bloat/...',
    '//pw_build/...',
    '//pw_checksum/...',
    '//pw_chrono_embos/...',
    '//pw_chrono_freertos/...',
    '//pw_chrono_stl/...',
    '//pw_chrono_threadx/...',
    '//pw_cli/...',
    '//pw_containers/...',
    '//pw_cpu_exception/...',
    '//pw_docgen/...',
    '//pw_doctor/...',
    '//pw_env_setup/...',
    '//pw_fuzzer/...',
    '//pw_hex_dump/...',
    '//pw_i2c/...',
    '//pw_interrupt/...',
    '//pw_interrupt_cortex_m/...',
    '//pw_libc/...',
    '//pw_log/...',
    '//pw_log_basic/...',
    '//pw_malloc/...',
    '//pw_malloc_freelist/...',
    '//pw_multisink/...',
    '//pw_polyfill/...',
    '//pw_preprocessor/...',
    '//pw_protobuf/...',
    '//pw_protobuf_compiler/...',
    '//pw_random/...',
    '//pw_result/...',
    '//pw_rpc/...',
    '//pw_span/...',
    '//pw_status/...',
    '//pw_stream/...',
    '//pw_string/...',
    '//pw_sync_baremetal/...',
    '//pw_sync_embos/...',
    '//pw_sync_freertos/...',
    '//pw_sync_stl/...',
    '//pw_sync_threadx/...',
    '//pw_sys_io/...',
    '//pw_sys_io_baremetal_lm3s6965evb/...',
    '//pw_sys_io_baremetal_stm32f429/...',
    '//pw_sys_io_stdio/...',
    '//pw_thread_stl/...',
    '//pw_tool/...',
    '//pw_toolchain/...',
    '//pw_transfer/...',
    '//pw_unit_test/...',
    '//pw_varint/...',
    '//pw_web_ui/...',
]

# TODO(pwbug/180): Slowly add modules here that work with bazel until all
# modules are added. Then replace with //...
_MODULES_THAT_TEST_WITH_BAZEL = [
    '//pw_allocator/...',
    '//pw_analog/...',
    '//pw_assert/...',
    '//pw_base64/...',
    '//pw_checksum/...',
    '//pw_cli/...',
    '//pw_containers/...',
    '//pw_hex_dump/...',
    '//pw_i2c/...',
    '//pw_libc/...',
    '//pw_log/...',
    '//pw_multisink/...',
    '//pw_polyfill/...',
    '//pw_preprocessor/...',
    '//pw_protobuf/...',
    '//pw_protobuf_compiler/...',
    '//pw_random/...',
    '//pw_result/...',
    '//pw_rpc/...',
    '//pw_span/...',
    '//pw_status/...',
    '//pw_stream/...',
    '//pw_string/...',
    '//pw_thread_stl/...',
    '//pw_unit_test/...',
    '//pw_varint/...',
    '//:buildifier_test',
]


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.bazel', '.bzl',
                        'BUILD'))
def bazel_test(ctx: PresubmitContext) -> None:
    """Runs bazel test on each bazel compatible module"""
    build.bazel(ctx, 'test', *_MODULES_THAT_TEST_WITH_BAZEL,
                '--test_output=errors')


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.bazel', '.bzl',
                        'BUILD'))
def bazel_build(ctx: PresubmitContext) -> None:
    """Runs Bazel build on each Bazel compatible module."""
    build.bazel(ctx, 'build', *_MODULES_THAT_BUILD_WITH_BAZEL)


#
# General presubmit checks
#


def _clang_system_include_paths(lang: str) -> List[str]:
    """Generate default system header paths.

    Returns the list of system include paths used by the host
    clang installation.
    """
    # Dump system include paths with preprocessor verbose.
    command = [
        'clang++', '-Xpreprocessor', '-v', '-x', f'{lang}', f'{os.devnull}',
        '-fsyntax-only'
    ]
    process = subprocess.run(command,
                             check=True,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)

    # Parse the command output to retrieve system include paths.
    # The paths are listed one per line.
    output = process.stdout.decode(errors='backslashreplace')
    include_paths: List[str] = []
    for line in output.splitlines():
        path = line.strip()
        if os.path.exists(path):
            include_paths.append(f'-isystem{path}')

    return include_paths


def edit_compile_commands(in_path: Path, out_path: Path,
                          func: Callable[[str, str, str], str]) -> None:
    """Edit the selected compile command file.

    Calls the input callback on all triplets (file, directory, command) in
    the input compile commands database. The return value replaces the old
    compile command in the output database.
    """
    with open(in_path) as in_file:
        compile_commands = json.load(in_file)
        for item in compile_commands:
            item['command'] = func(item['file'], item['directory'],
                                   item['command'])
    with open(out_path, 'w') as out_file:
        json.dump(compile_commands, out_file, indent=2)


# The first line must be regex because of the '20\d\d' date
COPYRIGHT_FIRST_LINE = r'Copyright 20\d\d The Pigweed Authors'
COPYRIGHT_COMMENTS = r'(#|//| \*|REM|::)'
COPYRIGHT_BLOCK_COMMENTS = (
    # HTML comments
    (r'<!--', r'-->'),
    # Jinja comments
    (r'{#', r'#}'),
)

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
    r'\bconstraint.list$',
    # Metadata
    r'^docker/tag$',
    r'\bAUTHORS$',
    r'\bLICENSE$',
    r'\bOWNERS$',
    r'\bPIGWEED_MODULES$',
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
    r'\.svg$',
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

        if path.is_dir():
            continue  # Skip submodules which are included in ctx.paths.

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
_GN_SOURCES_IN_BUILD = ('setup.cfg', '.toml', '.rst', '.py',
                        *_BAZEL_SOURCES_IN_BUILD)


@filter_paths(endswith=(*_GN_SOURCES_IN_BUILD, 'BUILD', '.bzl', '.gn', '.gni'),
              exclude=['zephyr.*/', 'android.*/'])
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


@filter_paths(endswith=(*format_code.C_FORMAT.extensions, '.py'))
def static_analysis(ctx: PresubmitContext):
    """Runs all available static analysis tools."""
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, 'python.lint', 'static_analysis')


def renode_check(ctx: PresubmitContext):
    """Placeholder for future check."""
    _LOG.info('%s %s', ctx.root, ctx.output_dir)


#
# Presubmit check programs
#

OTHER_CHECKS = (
    cpp_checks.all_sanitizers(),
    # Build that attempts to duplicate the build OSS-Fuzz does. Currently
    # failing.
    oss_fuzz_build,
    # TODO(pwbug/346): Enable all Bazel tests when they're fixed.
    bazel_test,
    cmake_clang,
    cmake_gcc,
    gn_boringssl_build,
    build.gn_gen_check,
    gn_nanopb_build,
    gn_crypto_mbedtls_build,
    gn_crypto_boringssl_build,
    gn_crypto_micro_ecc_build,
    gn_software_update_build,
    gn_full_build_check,
    gn_full_qemu_check,
    gn_clang_build,
    gn_gcc_build,
    gn_pw_system_demo_build,
    renode_check,
    stm32f429i,
)

_LINTFORMAT = (
    commit_message_format,
    copyright_notice,
    format_code.presubmit_checks(),
    inclusive_language.inclusive_language.with_filter(
        exclude=(r'\byarn.lock$', )),
    cpp_checks.pragma_once,
    build.bazel_lint,
    source_is_in_build_files,
)

LINTFORMAT = (
    _LINTFORMAT,
    static_analysis,
    pw_presubmit.python_checks.check_python_versions,
    pw_presubmit.python_checks.gn_python_lint,
)

QUICK = (
    _LINTFORMAT,
    gn_quick_build_check,
    # TODO(pwbug/141): Re-enable CMake and Bazel for Mac after we have fixed the
    # the clang issues. The problem is that all clang++ invocations need the
    # two extra flags: "-nostdc++" and "${clang_prefix}/../lib/libc++.a".
    cmake_clang if sys.platform != 'darwin' else (),
)

FULL = (
    _LINTFORMAT,
    gn_host_build,
    gn_arm_build,
    gn_docs_build,
    gn_host_tools,
    bazel_test if sys.platform == 'linux' else (),
    bazel_build if sys.platform == 'linux' else (),
    # On Mac OS, system 'gcc' is a symlink to 'clang' by default, so skip GCC
    # host builds on Mac for now. Skip it on Windows too, since gn_host_build
    # already uses 'gcc' on Windows.
    gn_gcc_build if sys.platform not in ('darwin', 'win32') else (),
    # Windows doesn't support QEMU yet.
    gn_qemu_build if sys.platform != 'win32' else (),
    gn_qemu_clang_build if sys.platform != 'win32' else (),
    source_is_in_build_files,
    python_checks.gn_python_check,
    python_checks.gn_python_test_coverage,
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
        # TODO(pwbug/209, pwbug/386) inclusive-language: disable
        install_hook(__file__, 'pre-push',
                     ['--base', 'origin/master..HEAD', '--program', 'quick'],
                     Path.cwd())
        # TODO(pwbug/209, pwbug/386) inclusive-language: enable
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
