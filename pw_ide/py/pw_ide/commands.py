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
import sys
from typing import Optional

from pw_ide.cpp import (get_defined_available_targets, get_target,
                        process_compilation_database, set_target,
                        write_compilation_databases)

from pw_ide.exceptions import (BadCompDbException, InvalidTargetException,
                               MissingCompDbException,
                               UnsupportedPlatformException)

from pw_ide.python import get_python_venv_path

from pw_ide.settings import IdeSettings


def _print_current_target(settings: IdeSettings) -> None:
    print('Current C/C++ language server analysis target: '
          f'{get_target(settings)}\n')


def _print_defined_available_targets(settings: IdeSettings) -> None:
    print('C/C++ targets available for language server analysis:')

    for toolchain in sorted(get_defined_available_targets(settings)):
        print(f'\t{toolchain}')

    print('')


def _print_python_venv_path() -> None:
    print('Python virtual environment path: ' f'{get_python_venv_path()}\n')


def _print_unsupported_platform_error(msg: str = 'run') -> None:
    system = platform.system()
    system = 'None' if system == '' else system
    print(f'Failed to {msg} on this unsupported platform: {system}\n')


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


def cmd_python(should_get_venv_path: bool) -> None:
    """Configure Python IDE support for Pigweed projects."""

    if should_get_venv_path:
        try:
            _print_python_venv_path()
        except UnsupportedPlatformException:
            _print_unsupported_platform_error(
                'find Python virtual environment')
