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
import shlex
import subprocess
import sys
from typing import Optional

from pw_ide.cpp import CppCompilationDatabase, CppIdeFeaturesState

from pw_ide.exceptions import (BadCompDbException, InvalidTargetException,
                               MissingCompDbException)

from pw_ide.python import PythonPaths

from pw_ide.settings import IdeSettings


# TODO(b/246850113): Replace prints with pw_cli.logs
def _print_current_target(settings: IdeSettings) -> None:
    print('Current C/C++ language server analysis target: '
          f'{CppIdeFeaturesState(settings).current_target}\n')


def _print_enabled_available_targets(settings: IdeSettings) -> None:
    print('C/C++ targets available for language server analysis:')

    for toolchain in sorted(
            CppIdeFeaturesState(settings).enabled_available_targets):
        print(f'\t{toolchain}')

    print('')


def _print_python_venv_path() -> None:
    print('Python virtual environment path: ' f'{PythonPaths().interpreter}\n')


def _make_working_dir(settings: IdeSettings, quiet: bool = False) -> None:
    if not settings.working_dir.exists():
        settings.working_dir.mkdir()
        print('Initialized the Pigweed IDE working directory at '
              f'{settings.working_dir}')
    else:
        if not quiet:
            print('Pigweed IDE working directory already present at '
                  f'{settings.working_dir}')


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
    _make_working_dir(settings)
    default = True

    if should_list_targets:
        default = False
        _print_enabled_available_targets(settings)

    # Order of operations matters here. It should be possible to process a
    # compilation database then set successfully set the target in a single
    # command.
    if compdb_file_path is not None:
        default = False
        try:
            CppCompilationDatabase\
                .load(compdb_file_path)\
                .process(
                    settings=settings,
                    path_globs=settings.clangd_query_drivers(),
                ).write()

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
        should_set_target = (
            CppIdeFeaturesState(settings).current_target is None
            or override_current_target)

        if should_set_target:
            try:
                CppIdeFeaturesState(settings).current_target = target_to_set
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
    _make_working_dir(settings)

    if should_get_venv_path:
        _print_python_venv_path()


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
