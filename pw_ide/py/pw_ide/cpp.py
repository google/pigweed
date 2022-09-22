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
"""Configure C/C++ IDE support for Pigweed projects."""

from collections import defaultdict
from dataclasses import dataclass, field
from io import TextIOBase
from inspect import cleandoc
import json
import os
from pathlib import Path
import platform
import re
import stat
from typing import (cast, Dict, Generator, List, Optional, Tuple, TypedDict,
                    Union)

from pw_ide.exceptions import (BadCompDbException, InvalidTargetException,
                               MissingCompDbException,
                               UnsupportedPlatformException)

from pw_ide.settings import IdeSettings
from pw_ide.symlinks import set_symlink

_COMPDB_FILE_PREFIX = 'compile_commands'
_COMPDB_FILE_SEPARATOR = '_'
_COMPDB_FILE_EXTENSION = '.json'

_COMPDB_CACHE_DIR_PREFIX = '.cache'
_COMPDB_CACHE_DIR_SEPARATOR = '_'

_SUPPORTED_TOOLCHAIN_EXECUTABLES = ('clang', 'gcc', 'g++')

COMPDB_FILE_GLOB = f'{_COMPDB_FILE_PREFIX}*{_COMPDB_FILE_EXTENSION}'
COMPDB_CACHE_DIR_GLOB = f'{_COMPDB_CACHE_DIR_PREFIX}*'

CLANGD_WRAPPER_FILE_NAME = 'clangd'


def _target_and_executable_from_command(
        command: str) -> Tuple[Optional[str], Optional[str]]:
    """Extract the target and executable name from a compile command."""

    tokens = command.split(' ')
    target: Optional[str] = None
    executable: Optional[str] = Path(tokens[0]).name
    executable = executable if executable != '' else None

    if len(tokens) > 1:
        for token in tokens[1:]:
            # Skip all flags and whitespace until we find the first reference to
            # the actual file in the command. The top level directory of the
            # file is the target name.
            # TODO(chadnorvell): This might be too specific to GN.
            if not token.startswith('-') and not token.strip() == '':
                target = Path(token).parts[0]
                break

    # This is indicative of Python wrapper commands, but is also an artifact of
    # the unsophisticated way we extract the target here.
    if target in ('.', '..'):
        target = None

    return (target, executable)


class CppCompileCommandDict(TypedDict):
    file: str
    directory: str
    command: str


@dataclass(frozen=True)
class CppCompileCommand:
    """A representation of a clang compilation database compile command.

    See: https://clang.llvm.org/docs/JSONCompilationDatabase.html
    """

    file: str
    directory: str
    command: str
    target: Optional[str] = field(default=None, init=False)
    executable: Optional[str] = field(default=None, init=False)

    def __post_init__(self) -> None:
        (target,
         executable) = _target_and_executable_from_command(self.command)

        # We want this class to be essentially immutable, accomplished
        # by freezing it. But that means we need to resort to this
        # to set these attributes during init.
        object.__setattr__(self, 'executable', executable)
        object.__setattr__(self, 'target', target)

    def as_dict(self) -> CppCompileCommandDict:
        return {
            "file": self.file,
            "directory": self.directory,
            "command": self.command,
        }


LoadableToCppCompilationDatabase = Union[List[CppCompileCommandDict], str,
                                         TextIOBase, Path]


class CppCompilationDatabase:
    """A representation of a clang compilation database.

    See: https://clang.llvm.org/docs/JSONCompilationDatabase.html
    """
    def __init__(self) -> None:
        self._db: List[CppCompileCommand] = []

    def __len__(self) -> int:
        return len(self._db)

    def __getitem__(self, index) -> CppCompileCommand:
        return self._db[index]

    def __iter__(self) -> Generator[CppCompileCommand, None, None]:
        return (compile_command for compile_command in self._db)

    def add(self, command: CppCompileCommand):
        """Add a compile command to the compilation database."""

        self._db.append(command)

    def as_dicts(self) -> List[CppCompileCommandDict]:
        return [compile_command.as_dict() for compile_command in self._db]

    def to_json(self) -> str:
        """Output the compilation database to a JSON string."""

        return json.dumps(self.as_dicts(), indent=2, sort_keys=True)

    def to_file(self, path: Path):
        """Write the compilation database to a JSON file."""

        with open(path, 'w') as file:
            json.dump(self.as_dicts(), file, indent=2, sort_keys=True)

    @classmethod
    def load(
        cls, compdb_to_load: LoadableToCppCompilationDatabase
    ) -> 'CppCompilationDatabase':
        """Load a compilation database.

        You can provide a JSON file handle or path, a JSON string, or a native
        Python data structure that matches the format (list of dicts).
        """

        db_as_dicts: List[CppCompileCommandDict]

        if isinstance(compdb_to_load, list):
            # The provided data is already in the format we want it to be in,
            # probably, and if it isn't we'll find out when we try to
            # instantiate the database.
            db_as_dicts = compdb_to_load
        else:
            if isinstance(compdb_to_load, Path):
                # The provided data is a path to a file, presumably JSON.
                try:
                    compdb_data = compdb_to_load.read_text()
                except FileNotFoundError:
                    raise MissingCompDbException()
            elif isinstance(compdb_to_load, TextIOBase):
                # The provided data is a file handle, presumably JSON.
                compdb_data = compdb_to_load.read()
            elif isinstance(compdb_to_load, str):
                # The provided data is a a string, presumably JSON.
                compdb_data = compdb_to_load

            db_as_dicts = json.loads(compdb_data)

        compdb = cls()

        try:
            compdb._db = [
                CppCompileCommand(**compile_command)
                for compile_command in db_as_dicts
            ]
        except TypeError:
            # This will arise if db_as_dicts is not actually a list of dicts
            raise BadCompDbException()

        return compdb


def compdb_generate_file_path(target: str = '') -> Path:
    """Generate a compilation database file path."""

    path = Path(f'{_COMPDB_FILE_PREFIX}.json')

    if target:
        path = path.with_name(f'{_COMPDB_FILE_PREFIX}'
                              f'{_COMPDB_FILE_SEPARATOR}{target}'
                              f'{_COMPDB_FILE_EXTENSION}')

    return path


def compdb_generate_cache_file_path(target: str = '') -> Path:
    """Generate a compilation database cache directory path."""

    path = Path(f'{_COMPDB_CACHE_DIR_PREFIX}')

    if target:
        path = path.with_name(f'{_COMPDB_CACHE_DIR_PREFIX}'
                              f'{_COMPDB_CACHE_DIR_SEPARATOR}{target}')

    return path


def compdb_target_from_path(filename: Path) -> Optional[str]:
    """Given a path that contains a compilation database file name, return the
    name of the database's compilation target."""

    # The length of the common compilation database file name prefix
    prefix_length = len(_COMPDB_FILE_PREFIX) + len(_COMPDB_FILE_SEPARATOR)

    if len(filename.stem) <= prefix_length:
        return None

    if filename.stem[:prefix_length] != (_COMPDB_FILE_PREFIX +
                                         _COMPDB_FILE_SEPARATOR):
        return None

    return filename.stem[prefix_length:]


def _none_to_empty_str(value: Optional[str]) -> str:
    return value if value is not None else ''


def _none_if_not_exists(path: Path) -> Optional[Path]:
    return path if path.exists() else None


def _compdb_cache_path_if_exists(working_dir: Path,
                                 target: Optional[str]) -> Optional[Path]:
    return _none_if_not_exists(
        working_dir /
        compdb_generate_cache_file_path(_none_to_empty_str(target)))


def get_available_compdbs(
        settings: IdeSettings) -> List[Tuple[Path, Optional[Path]]]:
    """Return the paths of all compilations databases and their associated
    caches that exist in the working directory as tuples."""
    compdbs_with_targets = (
        (file_path, compdb_target_from_path(file_path))
        for file_path in settings.working_dir.iterdir()
        if file_path.match(f'{_COMPDB_FILE_PREFIX}*{_COMPDB_FILE_EXTENSION}'))

    compdbs_with_caches = []

    for file_path, target in compdbs_with_targets:
        if file_path.name != compdb_generate_file_path().name:
            compdbs_with_caches.append(
                (file_path,
                 _compdb_cache_path_if_exists(settings.working_dir, target)))

    return compdbs_with_caches


def get_available_targets(settings: IdeSettings) -> List[str]:
    """Get the names of all targets available for code analysis.

    The presence of compilation database files matching the expected filename
    format in the expected directory is the source of truth on what targets
    are available.
    """
    match_expr = (fr'^{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
                  fr'(\w+){_COMPDB_FILE_EXTENSION}$')

    targets = []

    for filename in settings.working_dir.iterdir():
        match = re.match(match_expr, filename.name)
        if match is not None:
            targets.append(match.group(1))

    return targets


def get_defined_available_targets(settings: IdeSettings) -> List[str]:
    """Get the names of all targets that are both available for code analysis
    and defined in the settings file as targets that should be visible to the
    user."""
    available_targets = get_available_targets(settings)

    if len(settings.targets) == 0:
        return available_targets

    return [
        target for target in available_targets if target in settings.targets
    ]


def _is_available_target(target: Optional[str], settings: IdeSettings) -> bool:
    """Determines if a target is available for code analysis.

    Availability is defined by the presence of a compilation database for the
    target in the working directory.
    """
    return target is not None and target in get_available_targets(settings)


def _is_valid_target(target: Optional[str], settings: IdeSettings) -> bool:
    """Determines if a target can be used for code analysis.

    By default, any target is valid. But the project or user settings can
    constrain the valid targets to some subset of available targets (e.g. to
    hide variations on the same target that are irrelevant to code analysis).
    """
    return target is not None and (len(settings.targets) == 0
                                   or target in settings.targets)


def _is_valid_executable(executable: Optional[str]) -> bool:
    """Determines if a compiler executable is valid for code analysis.

    We assume it is if the executable name contains the name of one of the
    declared supported toolchains.
    """
    if executable is None:
        return False

    for supported_executable in _SUPPORTED_TOOLCHAIN_EXECUTABLES:
        if supported_executable in executable:
            return True

    return False


def _is_valid_target_and_executable(compile_command: CppCompileCommand,
                                    settings: IdeSettings) -> bool:
    """Determines if a compile command has a target and executable combination
    that can be used with code analysis."""

    return _is_valid_target(compile_command.target,
                            settings) and (_is_valid_executable(
                                compile_command.executable))


def get_target(settings: IdeSettings) -> Optional[str]:
    """Get the name of the current target used for code analysis.

    The presence of a symlink with the expected filename pointing to a
    compilation database matching the expected filename format is the source of
    truth on what the current target is.
    """
    try:
        # TODO(b/248257406) Use Path.readlink after dropping Python 3.8 support.
        src_file = Path(
            os.readlink(settings.working_dir / compdb_generate_file_path()))
    except (FileNotFoundError, OSError):
        # If the symlink doesn't exist, there is no current target.
        return None

    return compdb_target_from_path(Path(src_file))


def set_target(target: str, settings: IdeSettings) -> None:
    """Set the target that will be used for code analysis."""

    if not _is_valid_target(target, settings):
        raise InvalidTargetException()

    compdb_symlink_path = settings.working_dir / compdb_generate_file_path()

    compdb_target_path = (settings.working_dir /
                          compdb_generate_file_path(target))

    if not compdb_target_path.exists():
        raise MissingCompDbException()

    set_symlink(compdb_target_path, compdb_symlink_path)

    cache_symlink_path = (settings.working_dir /
                          compdb_generate_cache_file_path())

    cache_target_path = (settings.working_dir /
                         compdb_generate_cache_file_path(target))

    if not cache_target_path.exists():
        os.mkdir(cache_target_path)

    set_symlink(cache_target_path, cache_symlink_path)


def aggregate_compilation_database_targets(
        compdb_file: LoadableToCppCompilationDatabase) -> List[str]:
    """Given a clang compilation database, return all unique targets."""

    compdb = CppCompilationDatabase.load(compdb_file)
    targets = set()

    for compile_command in compdb:
        if compile_command.target is not None:
            targets.add(compile_command.target)

    return list(targets)


def process_compilation_database(
        compdb_file: LoadableToCppCompilationDatabase,
        settings: IdeSettings) -> Dict[str, CppCompilationDatabase]:
    """Given a clang compilation database that may have commands for multiple
    valid or invalid targets/toolchains, keep only the valid compile commands
    and store them in target-specific compilation databases."""

    raw_compdb = CppCompilationDatabase.load(compdb_file)
    clean_compdbs: Dict[str, CppCompilationDatabase] = (
        defaultdict(CppCompilationDatabase))

    for compile_command in raw_compdb:
        if _is_valid_target_and_executable(compile_command, settings):
            # If target is None, we won't arrive here.
            target = cast(str, compile_command.target)
            clean_compdbs[target].add(compile_command)

    return clean_compdbs


def write_compilation_databases(compdbs: Dict[str, CppCompilationDatabase],
                                settings: IdeSettings) -> None:
    """Write compilation databases to target-specific JSON files."""

    for target, compdb in compdbs.items():
        compdb.to_file(settings.working_dir /
                       compdb_generate_file_path(target))


def make_clangd_script(system: str = platform.system()) -> str:
    """Create a clangd wrapper script appropriate to the platform this is
    running on."""

    boilerplate = """
        # A wrapper around clangd that ensures it is run in the activated
        # Pigweed environment, which in turn ensures that the correct toolchain
        # paths are used.
        # THIS FILE IS AUTOMATICALLY GENERATED AND SHOULDN'T BE MODIFIED!"""

    posix_script = cleandoc(f"""
        #!/bin/bash
        {boilerplate}

        CWD="$(pwd)"

        if [ ! -f "$CWD/activate.sh" ]; then
            echo "clangd: must be run from workspace root (currently in $CWD)" 1>&2
            exit 1
        fi

        if [ ! -d "$CWD/environment" ]; then
            echo "clangd: Pigweed must be bootstrapped" 1>&2
            exit 1
        fi

        [[ -z "$PW_ROOT" ]] && source "$CWD/activate.sh" >/dev/null 2>&1

        exec "$PW_PIGWEED_CIPD_INSTALL_DIR/bin/clangd" --query-driver="$PW_PIGWEED_CIPD_INSTALL_DIR/bin/*,$PW_ARM_CIPD_INSTALL_DIR/bin/*" "$@"
""")

    windows_script = cleandoc(f"""
        :<<"::WINDOWS_ONLY"
        @echo off
        :<<"::WINDOWS_ONLY"
        {boilerplate.replace('#', '::')}

        if not exist "%cd%\\activate.sh" {{
            echo clangd: must be run from workspace root (currently in %cd%) 1>&2
            exit /b 2
        }}

        if not exist "%cd%\\environment" {{
            echo clangd: Pigweed must be bootstrapped 1>&2
            exit /b 2
        }}

        if "%PW_ROOT%" == "" {{
            %cd%\\activate.bat >nul 2>&1
        }}

        start "%PW_PIGWEED_CIPD_INSTALL_DIR%\\bin\\clangd" --query-driver="%PW_PIGWEED_CIPD_INSTALL_DIR%\\bin\\*,%PW_ARM_CIPD_INSTALL_DIR%\\bin\\*" "%*"
        ::WINDOWS_ONLY
""")

    if system == '':
        raise UnsupportedPlatformException()

    if system == 'Windows':
        # On Windows, use a batch script.
        return windows_script

    # If it's not Windows, assume a POSIX shell script will work.
    return posix_script


def write_clangd_wrapper_script(script: str, working_dir: Path) -> None:
    """Write a clangd wrapper script to file and make it executable.

    clangd needs to run in the activated environment to detect toolchains
    correctly, so we wrap it in this script instead of calling it directly.
    This also allows us to abstract over the actual environment directory.
    """

    wrapper_path = working_dir / CLANGD_WRAPPER_FILE_NAME

    with wrapper_path.open('w') as wrapper_file:
        wrapper_file.write(script)

    current_stat = wrapper_path.stat()
    # This is `chmod +x`.
    wrapper_path.chmod(current_stat.st_mode | stat.S_IEXEC)
