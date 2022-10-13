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
"""Configure C/C++ IDE support for Pigweed projects.

We support C/C++ code analysis via ``clangd``, or other language servers that
are compatible with the ``clangd`` compilation database format.

While clangd can work well out of the box for typical C++ codebases, some work
is required to coax it to work for embedded projects. In particular, Pigweed
projects use multiple toolchains within a distinct environment, and almost
always define multiple targets. This means compilation units are likely have
multiple compile commands and the toolchain executables are unlikely to be in
your path. ``clangd`` is not equipped to deal with this out of the box. We
handle this by:

- Processing the compilation database produced by the build system into
  multiple internally-consistent compilation databases, one for each target
  (where a "target" is a particular build for a particular system using a
  particular toolchain).

- Creating unambiguous paths to toolchain drivers to ensure the right toolchain
  is used and that clangd knows where to find that toolchain's system headers.

- Providing tools for working with several compilation databases that are
  spiritually similar to tools like ``pyenv``, ``rbenv``, etc.

In short, we take the probably-broken compilation database that the build system
generates, process it into several not-broken compilation databases in the
``pw_ide`` working directory, and provide a stable symlink that points to the
selected active target's compliation database. If ``clangd`` is configured to
point at the symlink and is set up with the right paths, you'll get code
intelligence.
"""

from collections import defaultdict
from dataclasses import dataclass
import glob
from io import TextIOBase
import json
import os
from pathlib import Path
import platform
from typing import (cast, Callable, Dict, Generator, List, Optional, Set,
                    Tuple, TypedDict, Union)

from pw_ide.exceptions import (BadCompDbException, InvalidTargetException,
                               MissingCompDbException,
                               UnresolvablePathException)

from pw_ide.settings import IdeSettings, PW_PIGWEED_CIPD_INSTALL_DIR
from pw_ide.symlinks import set_symlink

_COMPDB_FILE_PREFIX = 'compile_commands'
_COMPDB_FILE_SEPARATOR = '_'
_COMPDB_FILE_EXTENSION = '.json'

_COMPDB_CACHE_DIR_PREFIX = '.cache'
_COMPDB_CACHE_DIR_SEPARATOR = '_'

COMPDB_FILE_GLOB = f'{_COMPDB_FILE_PREFIX}*{_COMPDB_FILE_EXTENSION}'
COMPDB_CACHE_DIR_GLOB = f'{_COMPDB_CACHE_DIR_PREFIX}*'

_SUPPORTED_TOOLCHAIN_EXECUTABLES = ('clang', 'gcc', 'g++')


def compdb_generate_file_path(target: str = '') -> Path:
    """Generate a compilation database file path."""

    path = Path(f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_EXTENSION}')

    if target:
        path = path.with_name(f'{_COMPDB_FILE_PREFIX}'
                              f'{_COMPDB_FILE_SEPARATOR}{target}'
                              f'{_COMPDB_FILE_EXTENSION}')

    return path


def compdb_generate_cache_path(target: str = '') -> Path:
    """Generate a compilation database cache directory path."""

    path = Path(f'{_COMPDB_CACHE_DIR_PREFIX}')

    if target:
        path = path.with_name(f'{_COMPDB_CACHE_DIR_PREFIX}'
                              f'{_COMPDB_CACHE_DIR_SEPARATOR}{target}')

    return path


def compdb_target_from_path(filename: Path) -> Optional[str]:
    """Get a target name from a compilation database path."""

    # The length of the common compilation database file name prefix
    prefix_length = len(_COMPDB_FILE_PREFIX) + len(_COMPDB_FILE_SEPARATOR)

    if len(filename.stem) <= prefix_length:
        # This will return None for the symlink filename, and any filename that
        # is too short to be a compilation database.
        return None

    if filename.stem[:prefix_length] != (_COMPDB_FILE_PREFIX +
                                         _COMPDB_FILE_SEPARATOR):
        # This will return None for any files that don't have the common prefix.
        return None

    return filename.stem[prefix_length:]


def _none_to_empty_str(value: Optional[str]) -> str:
    return value if value is not None else ''


def _none_if_not_exists(path: Path) -> Optional[Path]:
    return path if path.exists() else None


def compdb_cache_path_if_exists(working_dir: Path,
                                target: Optional[str]) -> Optional[Path]:
    return _none_if_not_exists(
        working_dir / compdb_generate_cache_path(_none_to_empty_str(target)))


def target_is_enabled(target: Optional[str], settings: IdeSettings) -> bool:
    """Determine if a target is enabled.

    By default, all targets are enabled. If specific targets are defined in a
    settings file, only those targets will be enabled.
    """

    if target is None:
        return False

    if len(settings.targets) == 0:
        return True

    return target in settings.targets


def path_to_executable(exe: str,
                       *,
                       default_path: Optional[Path] = None,
                       path_globs: Optional[List[str]] = None,
                       strict: bool = False) -> Optional[Path]:
    """Return the path to a compiler executable.

    In a ``clang`` compile command, the executable may or may not include a
    path. For example:

    .. code-block:: none

       /usr/bin/clang      <- includes a path
       ../path/to/my/clang <- includes a path
       clang               <- doesn't include a path

    If it includes a path, then ``clangd`` will have no problem finding the
    driver, so we can simply return the path. If the executable *doesn't*
    include a path, then ``clangd`` will search ``$PATH``, and may not find the
    intended driver unless you actually want the default system toolchain or
    Pigweed paths have been added to ``$PATH``. So this function provides two
    options for resolving those ambiguous paths:

    - Provide a default path, and all executables without a path will be
      re-written with a path within the default path.
    - Provide the a set of globs that will be used to search for the executable,
      which will normally be the query driver globs used with clangd.

    By default, if neither of these options is chosen, or if the executable
    cannot be found within the provided globs, the pathless executable that was
    provided will be returned, and clangd will resort to searching $PATH. If you
    instead pass ``strict=True``, this will raise an exception if an unambiguous
    path cannot be constructed.

    This function only tries to ensure that all executables have a path to
    eliminate ambiguity. A couple of important things to keep in mind:

    - This doesn't guarantee that the path exists or an executable actually
      exists at the path. It only ensures that some path is provided to an
      executable.
    - An executable being present at the indicated path doesn't guarantee that
      it will work flawlessly for clangd code analysis. The clangd
      ``--query-driver`` argument needs to include a path to this executable in
      order for its bundled headers to be resolved correctly.

    This function also filters out invalid or unsupported drivers. For example,
    build systems will sometimes naively include build steps for Python or other
    languages in the compilation database, which are not usable with clangd.
    As a result, this function has four possible end states:

    - It returns a path with an executable that can be used as a ``clangd``
      driver.
    - It returns ``None``, meaning the compile command was invalid.
    - It returns the same string that was provided (as a ``Path``), if a path
      couldn't be resolved and ``strict=False``.
    - It raises an ``UnresolvablePathException`` if the executable cannot be
      placed in an unambiguous path and ``strict=True``.
    """
    maybe_path = Path(exe)

    # We were give an empty string -- not a path.
    if len(maybe_path.parts) == 0:
        raise UnresolvablePathException('Invalid path provided!')

    # Determine if the executable name matches supported drivers.
    is_supported_driver = False

    for supported_executable in _SUPPORTED_TOOLCHAIN_EXECUTABLES:
        if supported_executable in maybe_path.name:
            is_supported_driver = True

    if not is_supported_driver:
        return None

    # Now, ensure the executable has a path.

    # This is either a relative or absolute path -- return it.
    if len(maybe_path.parts) > 1:
        return maybe_path

    # If we got here, there's only one "part", so we assume it's an executable
    # without a path. This logic doesn't work with a path like `./exe` since
    # that also yields only one part. So currently this breaks if you actually
    # have your compiler executable in your root build directory, which is
    # (hopefully) very rare.

    # If we got a default path, use it.
    if default_path is not None:
        return default_path / maybe_path

    # Otherwise, try to find the executable within the query driver globs.
    # Note that unlike the previous paths, this path will only succeed if an
    # executable actually exists somewhere in the query driver globs.
    if path_globs is not None:
        for path_glob in path_globs:
            for path_str in glob.iglob(path_glob):
                path = Path(path_str)
                if path.name == maybe_path.name:
                    return path.absolute()

    if strict:
        raise UnresolvablePathException(
            f'Cannot place {exe} in an unambiguous path!')

    return maybe_path


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
    target: Optional[str] = None

    @property
    def command_parts(self) -> Tuple[str, List[str]]:
        """Return the executable string and the rest of the command tokens."""
        try:
            parts = self.command.split(' ')
            return parts[0], parts[1:]
        except IndexError:
            raise RuntimeError(
                'Failed to load malformed compilation database!')

    @property
    def executable_name(self) -> str:
        return Path(self.command_parts[0]).name

    def process(
        self,
        *,
        default_path: Optional[Path] = None,
        path_globs: Optional[List[str]] = None,
        strict: bool = False,
    ) -> Optional['CppCompileCommand']:
        """Process a compile command.

        At minimum, a compile command from a clang compilation database needs to
        be correlated with its target, and this method returns the target name
        with the compile command. But it also cleans up other things we need for
        reliable code intelligence:

        - Some targets may not be valid C/C++ compile commands. For example,
          some build systems will naively include build steps for Python or for
          linting commands. We want to filter those out.

        - Some compile commands don't provide a path to the compiler executable
          (referred to by clang as the "driver"). In that case, clangd is very
          unlikely to find the executable unless it happens to be in ``$PATH``.
          The ``--query-driver`` argument to ``clangd`` allowlists
          executables/drivers for use its use, but clangd doesn't use it to
          resolve ambiguous paths. We bridge that gap here. Any executable
          without a path will be either placed in the provided default path or
          searched for in the query driver globs and be replaced with a path to
          the executable.
        """
        executable_str, tokens = self.command_parts
        executable_path = path_to_executable(executable_str,
                                             default_path=default_path,
                                             path_globs=path_globs,
                                             strict=strict)

        target: Optional[str] = None

        if len(tokens) > 1:
            for token in tokens:
                # Skip all flags and whitespace until we find the first
                # reference to the actual file in the command. The top level
                # directory of the file is the target name.
                # TODO(chadnorvell): This might be too specific to GN.
                if not token.startswith('-') and not token.strip() == '':
                    target = Path(token).parts[0]
                    break

        # The latter condition is indicative of Python wrapper commands, but is
        # also an artifact of the unsophisticated way we extract the target.
        if executable_path is None or target is None or target in ('.', '..'):
            return None

        new_command = f'{str(executable_path)} {" ".join(tokens)}'

        return self.__class__(
            file=self.file,
            directory=self.directory,
            command=new_command,
            target=target,
        )

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

    def __getitem__(self, index: int) -> CppCompileCommand:
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

    def process(
        self,
        settings: IdeSettings,
        *,
        default_path: Optional[Path] = None,
        path_globs: Optional[List[str]] = None,
        strict: bool = False,
    ) -> 'CppCompilationDatabasesMap':
        """Process a ``clangd`` compilation database file.

        Given a clang compilation database that may have commands for multiple
        valid or invalid targets/toolchains, keep only the valid compile
        commands and store them in target-specific compilation databases.
        """
        clean_compdbs = CppCompilationDatabasesMap(settings)

        for compile_command in self:
            processed_compile_command = compile_command.process(
                default_path=default_path,
                path_globs=path_globs,
                strict=strict)

            if (processed_compile_command is not None and target_is_enabled(
                    processed_compile_command.target, settings)):
                # This invariant is satisfied by target_is_enabled
                target = cast(str, processed_compile_command.target)
                clean_compdbs[target].add(processed_compile_command)

        return clean_compdbs


class CppCompilationDatabasesMap:
    """Container for a map of target name to compilation database."""
    def __init__(self, settings: IdeSettings):
        self.settings = settings
        self._dbs: Dict[str, CppCompilationDatabase] = (
            defaultdict(CppCompilationDatabase))

    def __getitem__(self, key: str) -> CppCompilationDatabase:
        return self._dbs[key]

    def __setitem__(self, key: str, item: CppCompilationDatabase) -> None:
        self._dbs[key] = item

    def items(
            self) -> Generator[Tuple[str, CppCompilationDatabase], None, None]:
        return ((key, value) for (key, value) in self._dbs.items())

    def write(self) -> None:
        """Write compilation databases to target-specific JSON files."""

        for target, compdb in self.items():
            compdb.to_file(self.settings.working_dir /
                           compdb_generate_file_path(target))


@dataclass(frozen=True)
class CppIdeFeaturesTarget:
    """Data pertaining to a C++ code analysis target."""
    name: str
    compdb_file_path: Path
    compdb_cache_path: Optional[Path]
    is_enabled: bool


class CppIdeFeaturesState:
    """The state of the C++ analysis targets in the working directory.

    Targets can be:

    - **Available**: A compilation database is present for this target.
    - **Enabled**: Any targets are enabled by default, but a subset can be
      enabled instead in the pw_ide settings. Enabled targets need
      not be available if they haven't had a compilation database
      created through processing yet.
    - **Valid**: Is both available and enabled.
    - **Current**: The one currently activated target that is exposed to clangd.
    """
    def __init__(self, settings: IdeSettings) -> None:
        self.settings = settings

        # We filter out Nones below, so we can assume its a str
        target: Callable[[Path], str] = (
            lambda path: cast(str, compdb_target_from_path(path)))

        # Contains every compilation database that's present in the working dir.
        # This dict comprehension looks monstrous, but it just finds targets and
        # associates the target names with their CppIdeFeaturesTarget objects.
        self.targets: Dict[str, CppIdeFeaturesTarget] = {
            target(file_path): CppIdeFeaturesTarget(
                name=target(file_path),
                compdb_file_path=file_path,
                compdb_cache_path=compdb_cache_path_if_exists(
                    settings.working_dir, compdb_target_from_path(file_path)),
                is_enabled=target_is_enabled(target(file_path), settings))
            for file_path in settings.working_dir.iterdir() if
            file_path.match(f'{_COMPDB_FILE_PREFIX}*{_COMPDB_FILE_EXTENSION}')
            # This filters out the symlink
            and compdb_target_from_path(file_path) is not None
        }

        # Contains the currently selected target.
        self._current_target: Optional[CppIdeFeaturesTarget] = None

        # This is diagnostic data; it tells us what the current target should
        # be, even if the state of the working directory is corrupted and the
        # compilation database for the target isn't actually present. Anything
        # that requires a compilation database to be definitely present should
        # use `current_target` instead of these values.
        self.current_target_name: Optional[str] = None
        self.current_target_file_path: Optional[Path] = None
        self.current_target_exists: Optional[bool] = None

        try:
            src_file = Path(
                os.readlink(
                    (settings.working_dir / compdb_generate_file_path())))

            self.current_target_file_path = src_file
            self.current_target_name = compdb_target_from_path(src_file)

            if not self.current_target_file_path.exists():
                self.current_target_exists = False

            else:
                self.current_target_exists = True
                self._current_target = CppIdeFeaturesTarget(
                    name=target(src_file),
                    compdb_file_path=src_file,
                    compdb_cache_path=compdb_cache_path_if_exists(
                        settings.working_dir, target(src_file)),
                    is_enabled=target_is_enabled(target(src_file), settings),
                )
        except (FileNotFoundError, OSError):
            # If the symlink doesn't exist, there is no current target.
            pass

    def __len__(self) -> int:
        return len(self.targets)

    def __getitem__(self, index: str) -> CppIdeFeaturesTarget:
        return self.targets[index]

    def __iter__(self) -> Generator[CppIdeFeaturesTarget, None, None]:
        return (target for target in self.targets.values())

    @property
    def current_target(self) -> Optional[str]:
        """The name of current target used for code analysis.

        The presence of a symlink with the expected filename pointing to a
        compilation database matching the expected filename format is the source
        of truth on what the current target is.
        """
        return (self._current_target.name
                if self._current_target is not None else None)

    @current_target.setter
    def current_target(self, target: Optional[str]) -> None:
        settings = self.settings

        if not self.is_valid_target(target):
            raise InvalidTargetException()

        # The check above rules out None.
        target = cast(str, target)

        compdb_symlink_path = (settings.working_dir /
                               compdb_generate_file_path())

        compdb_target_path = (settings.working_dir /
                              compdb_generate_file_path(target))

        if not compdb_target_path.exists():
            raise MissingCompDbException()

        set_symlink(compdb_target_path, compdb_symlink_path)

        cache_symlink_path = (settings.working_dir /
                              compdb_generate_cache_path())

        cache_target_path = (settings.working_dir /
                             compdb_generate_cache_path(target))

        if not cache_target_path.exists():
            os.mkdir(cache_target_path)

        set_symlink(cache_target_path, cache_symlink_path)

    @property
    def available_targets(self) -> List[str]:
        return list(self.targets.keys())

    @property
    def enabled_available_targets(self) -> Generator[str, None, None]:
        return (name for name, target in self.targets.items()
                if target.is_enabled)

    def is_valid_target(self, target: Optional[str]) -> bool:
        if target is None or (data := self.targets.get(target, None)) is None:
            return False

        return data.is_enabled


def aggregate_compilation_database_targets(
        compdb_file: LoadableToCppCompilationDatabase,
        *,
        default_path: Optional[Path] = None,
        path_globs: Optional[List[str]] = None) -> List[str]:
    """Return all valid unique targets from a ``clang`` compilation database."""

    compdb = CppCompilationDatabase.load(compdb_file)
    targets: Set[str] = set()

    for compile_command in compdb:
        processed_compile_command = compile_command.process(
            default_path=default_path, path_globs=path_globs)

        if processed_compile_command is not None:
            # Target is always present in a processed compile command.
            target = cast(str, processed_compile_command.target)
            targets.add(target)

    return list(targets)


def delete_compilation_databases(settings: IdeSettings) -> None:
    """Delete all compilation databases in the working directory.

    This leaves cache directories in place.
    """
    for path in settings.working_dir.iterdir():
        if path.name.startswith(_COMPDB_FILE_PREFIX):
            path.unlink()


def delete_compilation_database_caches(settings: IdeSettings) -> None:
    """Delete all compilation database caches in the working directory.

    This leaves all compilation databases in place.
    """
    for path in settings.working_dir.iterdir():
        if path.name.startswith(_COMPDB_CACHE_DIR_PREFIX):
            path.unlink()


class ClangdSettings:
    """Makes system-specific settings for running ``clangd`` with Pigweed."""
    def __init__(self, settings: IdeSettings):
        self.compile_commands_dir: Path = IdeSettings().working_dir
        self.clangd_path: Path = Path(
            PW_PIGWEED_CIPD_INSTALL_DIR) / 'bin' / 'clangd'

        self.arguments: List[str] = [
            f'--compile-commands-dir={self.compile_commands_dir}',
            f'--query-driver={settings.clangd_query_driver_str()}',
            '--background-index', '--clang-tidy'
        ]

    def command(self, system: str = platform.system()) -> str:
        """Return the command that runs clangd with Pigweed paths."""
        def make_command(line_continuation: str):
            arguments = f' {line_continuation}\n'.join(
                f'  {arg}' for arg in self.arguments)
            return f'\n{self.clangd_path} {line_continuation}\n{arguments}'

        if system.lower() == 'json':
            return '\n' + json.dumps([str(self.clangd_path), *self.arguments],
                                     indent=2)

        if system.lower() in ['cmd', 'batch']:
            return make_command('`')

        if system.lower() in ['powershell', 'pwsh']:
            return make_command('^')

        if system.lower() == 'windows':
            return (f'\nIn PowerShell:\n{make_command("`")}'
                    f'\n\nIn Command Prompt:\n{make_command("^")}')

        # Default case for *sh-like shells.
        return make_command('\\')
