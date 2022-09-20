# Copyright 2022 The Pigweed Authors
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
"""pw_ide CLI command implementations."""

from pathlib import Path
import platform
import shlex
import subprocess
import sys
from typing import Callable, Optional

from pw_ide.cpp import (CLANGD_WRAPPER_FILE_NAME,
                        aggregate_compilation_database_targets,
                        get_available_compdbs, get_available_targets,
                        get_defined_available_targets, get_target,
                        make_clangd_script, process_compilation_database,
                        set_target, write_compilation_databases,
                        write_clangd_wrapper_script)

from pw_ide.exceptions import (BadCompDbException, InvalidTargetException,
                               MissingCompDbException,
                               UnsupportedPlatformException)

from pw_ide.python import (PYTHON_SYMLINK_NAME, create_python_symlink,
                           get_python_venv_path)

from pw_ide.settings import IdeSettings


# TODO(b/246850113): Replace prints with pw_cli.logs
def _print_working_dir(settings: IdeSettings) -> None:
    print(f'Pigweed IDE working directory: {str(settings.working_dir)}\n')


def _print_current_target(settings: IdeSettings) -> None:
    print('Current C/C++ language server analysis target: '
          f'{get_target(settings)}\n')


def _print_defined_targets(settings: IdeSettings) -> None:
    print('C/C++ targets defined in .pw_ide.yaml:')

    for target in settings.targets:
        print(f'\t{target}')

    print('')


def _print_available_targets(settings: IdeSettings) -> None:
    print('C/C++ targets available for language server analysis:')

    for toolchain in sorted(get_available_targets(settings)):
        print(f'\t{toolchain}')

    print('')


def _print_defined_available_targets(settings: IdeSettings) -> None:
    print('C/C++ targets available for language server analysis:')

    for toolchain in sorted(get_defined_available_targets(settings)):
        print(f'\t{toolchain}')

    print('')


def _print_available_compdbs(settings: IdeSettings) -> None:
    print('C/C++ compilation databases in the working directory:')

    for compdb, cache in get_available_compdbs(settings):
        output = compdb.name

        if cache is not None:
            output += f'\n\t\tcache: {str(cache.name)}'

        print(f'\t{output}')

    print('')


def _print_compdb_targets(compdb_file: Path) -> None:
    print(f'Unique targets in {str(compdb_file)}:')

    for target in sorted(aggregate_compilation_database_targets(compdb_file)):
        print(f'\t{target}')

    print('')


def _print_python_venv_path() -> None:
    print('Python virtual environment path: ' f'{get_python_venv_path()}\n')


def _print_unsupported_platform_error(msg: str = 'run') -> None:
    system = platform.system()
    system = 'None' if system == '' else system
    print(f'Failed to {msg} on this unsupported platform: {system}\n')


def cmd_info(available_compdbs: bool,
             available_targets: bool,
             defined_targets: bool,
             working_dir: bool,
             compdb_file_for_targets: Path = None,
             settings: IdeSettings = IdeSettings()):
    """Report diagnostic info about Pigweed IDE features."""
    if working_dir:
        _print_working_dir(settings)

    if defined_targets:
        _print_defined_targets(settings)

    if available_compdbs:
        _print_available_compdbs(settings)

    if available_targets:
        _print_available_targets(settings)

    if compdb_file_for_targets is not None:
        _print_compdb_targets(compdb_file_for_targets)


def cmd_init(
    make_dir: bool,
    make_clangd_wrapper: bool,
    make_python_symlink: bool,
    silent: bool = False,
    settings: IdeSettings = IdeSettings()) -> None:
    """Create IDE features working directory and supporting files.

    When called without arguments, this creates the Pigweed IDE features working
    directory defined in the settings file and ensures that further `pw_ide`
    commands work as expected by creating all the other IDE infrastructure.

    This command is idempotent, so it's safe to run it prophylactically or as a
    precursor to other commands to ensure that the Pigweed IDE features are in a
    working state.
    """

    maybe_print: Callable[[str], None] = print

    if silent:
        maybe_print = lambda _: None

    # If no flags were provided, do everything.
    if not make_dir and not make_clangd_wrapper and not make_python_symlink:
        make_dir = True
        make_clangd_wrapper = True
        make_python_symlink = True

    if make_dir:
        if not settings.working_dir.exists():
            settings.working_dir.mkdir()
            maybe_print('Initialized the Pigweed IDE working directory.')
        else:
            maybe_print('Pigweed IDE working directory already present.')

    if make_clangd_wrapper:
        clangd_wrapper_path = (settings.working_dir / CLANGD_WRAPPER_FILE_NAME)

        if not clangd_wrapper_path.exists():
            try:
                write_clangd_wrapper_script(make_clangd_script(),
                                            settings.working_dir)
            except UnsupportedPlatformException:
                _print_unsupported_platform_error('create clangd wrapper')
                sys.exit(1)

            maybe_print('Created a clangd wrapper script.')
        else:
            maybe_print('clangd wrapper script already present.')

    if make_python_symlink:
        python_symlink_path = settings.working_dir / PYTHON_SYMLINK_NAME

        if not python_symlink_path.exists():
            try:
                create_python_symlink(settings.working_dir)
            except UnsupportedPlatformException:
                _print_unsupported_platform_error('create Python symlink')
                sys.exit(1)

            maybe_print('Created Python symlink.')
        else:
            maybe_print('Python symlink already present.')


def cmd_cpp(
    should_list_targets: bool,
    target_to_set: Optional[str],
    compdb_file_path: Optional[Path],
    override_current_target: bool = True,
    settings: IdeSettings = IdeSettings()
) -> None:
    """Configure C/C++ IDE support for Pigweed projects.

    Provides tools for processing C/C++ compilation databases and setting the
    particular target/toochain to use for code analysis."""
    cmd_init(make_dir=True,
             make_clangd_wrapper=True,
             make_python_symlink=True,
             silent=True,
             settings=settings)
    default = True

    if should_list_targets:
        default = False
        _print_defined_available_targets(settings)

    # Order of operations matters here. It should be possible to process a
    # compilation database then set successfully set the target in a single
    # command.
    if compdb_file_path is not None:
        default = False
        try:
            write_compilation_databases(
                process_compilation_database(
                    compdb_file_path,
                    settings,
                ),
                settings,
            )

            print(
                f'Processed {str(compdb_file_path)} to {settings.working_dir}')
        except MissingCompDbException:
            print(f'File not found: {str(compdb_file_path)}')
            sys.exit(1)
        except BadCompDbException:
            print('File does not match compilation database format: '
                  f'{str(compdb_file_path)}')
            sys.exit(1)

    if target_to_set is not None:
        default = False
        should_set_target = get_target(settings) is None \
            or override_current_target

        if should_set_target:
            try:
                set_target(target_to_set, settings)
            except InvalidTargetException:
                print(f'Invalid target! {target_to_set} not among the '
                      'available defined targets.\n\n'
                      'Check .pw_ide.yaml or .pw_ide.user.yaml for defined '
                      'targets.')
                sys.exit(1)
            except MissingCompDbException:
                print(f'File not found for target! {target_to_set}\n'
                      'Did you run pw ide cpp --process '
                      '{path to compile_commands.json}?')
                sys.exit(1)

            print('Set C/C++ language server analysis target to: '
                  f'{target_to_set}')
        else:
            print('Target already is set and will not be overridden.')
            _print_current_target(settings)

    if default:
        _print_current_target(settings)


def cmd_python(
    should_get_venv_path: bool, settings: IdeSettings = IdeSettings()) -> None:
    """Configure Python IDE support for Pigweed projects."""
    cmd_init(make_dir=True,
             make_clangd_wrapper=True,
             make_python_symlink=True,
             silent=True,
             settings=settings)

    if should_get_venv_path:
        try:
            _print_python_venv_path()
        except UnsupportedPlatformException:
            _print_unsupported_platform_error(
                'find Python virtual environment')


def cmd_setup(settings: IdeSettings = IdeSettings()):
    """Set up or update your Pigweed project IDE features.

    This will execute all the commands needed to set up your development
    environment with all the features that Pigweed IDE supports, with sensible
    defaults. This command is idempotent, so run it whenever you feel like
    it."""

    if len(settings.setup) == 0:
        print('This project has no defined setup procedure :(\n'
              'Refer to the the pw_ide docs to learn how to define a setup!')
        sys.exit(1)

    for command in settings.setup:
        subprocess.run(shlex.split(command))
