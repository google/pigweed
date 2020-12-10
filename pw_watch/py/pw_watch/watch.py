#!/usr/bin/env python
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
"""Watch files for changes and rebuild.

pw watch runs Ninja in a build directory when source files change. It works with
any Ninja project (GN or CMake).

Usage examples:

  # Find a build directory and build the default target
  pw watch

  # Find a build directory and build the stm32f429i target
  pw watch python.lint stm32f429i

  # Build pw_run_tests.modules in the out/cmake directory
  pw watch -C out/cmake pw_run_tests.modules

  # Build the default target in out/ and pw_apps in out/cmake
  pw watch -C out -C out/cmake pw_apps

  # Find a directory and build python.tests, and build pw_apps in out/cmake
  pw watch python.tests -C out/cmake pw_apps
"""

import argparse
from dataclasses import dataclass
import logging
import os
from pathlib import Path
import shlex
import subprocess
import sys
import threading
from typing import (Iterable, List, NamedTuple, NoReturn, Optional, Sequence,
                    Tuple)

from watchdog.events import FileSystemEventHandler  # type: ignore
from watchdog.observers import Observer  # type: ignore

import pw_cli.branding
import pw_cli.color
import pw_cli.env
import pw_cli.plugins

from pw_watch.debounce import DebouncedFunction, Debouncer

_COLOR = pw_cli.color.colors()
_LOG = logging.getLogger(__name__)
_ERRNO_INOTIFY_LIMIT_REACHED = 28

_PASS_MESSAGE = """
  ██████╗  █████╗ ███████╗███████╗██╗
  ██╔══██╗██╔══██╗██╔════╝██╔════╝██║
  ██████╔╝███████║███████╗███████╗██║
  ██╔═══╝ ██╔══██║╚════██║╚════██║╚═╝
  ██║     ██║  ██║███████║███████║██╗
  ╚═╝     ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝
"""

# Pick a visually-distinct font from "PASS" to ensure that readers can't
# possibly mistake the difference between the two states.
_FAIL_MESSAGE = """
   ▄██████▒░▄▄▄       ██▓  ░██▓
  ▓█▓     ░▒████▄    ▓██▒  ░▓██▒
  ▒████▒   ░▒█▀  ▀█▄  ▒██▒ ▒██░
  ░▓█▒    ░░██▄▄▄▄██ ░██░  ▒██░
  ░▒█░      ▓█   ▓██▒░██░░ ████████▒
   ▒█░      ▒▒   ▓▒█░░▓  ░  ▒░▓  ░
   ░▒        ▒   ▒▒ ░ ▒ ░░  ░ ▒  ░
   ░ ░       ░   ▒    ▒ ░   ░ ░
                 ░  ░ ░       ░  ░
"""


# TODO(keir): Figure out a better strategy for exiting. The problem with the
# watcher is that doing a "clean exit" is slow. However, by directly exiting,
# we remove the possibility of the wrapper script doing anything on exit.
def _die(*args) -> NoReturn:
    _LOG.critical(*args)
    sys.exit(1)


class WatchCharset(NamedTuple):
    slug_ok: str
    slug_fail: str


_ASCII_CHARSET = WatchCharset(_COLOR.green('OK  '), _COLOR.red('FAIL'))
_EMOJI_CHARSET = WatchCharset('✔️ ', '💥')


@dataclass(frozen=True)
class BuildCommand:
    build_dir: Path
    targets: Tuple[str, ...] = ()

    def args(self) -> Tuple[str, ...]:
        return (str(self.build_dir), *self.targets)

    def __str__(self) -> str:
        return ' '.join(shlex.quote(arg) for arg in self.args())


class PigweedBuildWatcher(FileSystemEventHandler, DebouncedFunction):
    """Process filesystem events and launch builds if necessary."""
    def __init__(
        self,
        patterns: Sequence[str] = (),
        ignore_patterns: Sequence[str] = (),
        build_commands: Sequence[BuildCommand] = (),
        ignore_dirs=Optional[List[str]],
        charset: WatchCharset = _ASCII_CHARSET,
        restart: bool = False,
    ):
        super().__init__()

        self.patterns = patterns
        self.ignore_patterns = ignore_patterns
        self.build_commands = build_commands
        self.ignore_dirs = ignore_dirs or []
        self.ignore_dirs.extend(cmd.build_dir for cmd in self.build_commands)
        self.charset: WatchCharset = charset

        self.restart_on_changes = restart
        self._current_build: Optional[subprocess.Popen] = None

        self.debouncer = Debouncer(self)

        # Track state of a build. These need to be members instead of locals
        # due to the split between dispatch(), run(), and on_complete().
        self.matching_path: Optional[str] = None
        self.builds_succeeded: List[bool] = []

        self.wait_for_keypress_thread = threading.Thread(
            None, self._wait_for_enter)
        self.wait_for_keypress_thread.start()

    def _wait_for_enter(self):
        try:
            while True:
                _ = input()
                self.debouncer.press('Manual build requested...')
        # Ctrl-C on Unix generates KeyboardInterrupt
        # Ctrl-Z on Windows generates EOFError
        except (KeyboardInterrupt, EOFError):
            _exit_due_to_interrupt()

    def _path_matches(self, raw_path: str) -> bool:
        """Returns true if path matches according to the watcher patterns"""
        modified_path = Path(raw_path).resolve()

        # Check for modifications inside the ignore directories, and skip them.
        # Ideally these events would never hit the watcher, but selectively
        # watching directories at the OS level is not trivial due to limitations
        # of the watchdog module.
        for ignore_dir in self.ignore_dirs:
            resolved_ignore_dir = Path(ignore_dir).resolve()
            try:
                modified_path.relative_to(resolved_ignore_dir)
                # If no ValueError is raised by the .relative_to() call, then
                # this file is inside the ignore directory; so skip it.
                return False
            except ValueError:
                # Otherwise, the file isn't in the ignore directory, so run the
                # normal pattern checks below.
                pass

        return ((not any(modified_path.match(x) for x in self.ignore_patterns))
                and any(modified_path.match(x) for x in self.patterns))

    def dispatch(self, event) -> None:
        # There isn't any point in triggering builds on new directory creation.
        # It's the creation or modification of files that indicate something
        # meaningful enough changed for a build.
        if event.is_directory:
            return

        # Collect paths of interest from the event.
        paths: List[str] = []
        if hasattr(event, 'dest_path'):
            paths.append(os.fsdecode(event.dest_path))
        if event.src_path:
            paths.append(os.fsdecode(event.src_path))
        for path in paths:
            _LOG.debug('File event: %s', path)

        # Check for matching paths among the one or two in the event.
        for path in paths:
            if self._path_matches(path):
                self._handle_matched_event(path)
                return

    def _handle_matched_event(self, matching_path: str) -> None:
        if self.matching_path is None:
            self.matching_path = matching_path

        self.debouncer.press(
            f'File change detected: {os.path.relpath(matching_path)}')

    # Implementation of DebouncedFunction.run()
    #
    # Note: This will run on the timer thread created by the Debouncer, rather
    # than on the main thread that's watching file events. This enables the
    # watcher to continue receiving file change events during a build.
    def run(self):
        """Run all the builds in serial and capture pass/fail for each."""

        # Clear the screen and show a banner indicating the build is starting.
        print('\033c', end='')  # TODO(pwbug/38): Not Windows compatible.
        print(pw_cli.branding.banner())
        print(
            _COLOR.green(
                '  Watching for changes. Ctrl-C to exit; enter to rebuild'))
        print()
        _LOG.info('Change detected: %s', self.matching_path)

        self.builds_succeeded = []
        num_builds = len(self.build_commands)
        _LOG.info('Starting build with %d directories', num_builds)

        env = os.environ.copy()
        # Force colors in Pigweed subcommands run through the watcher.
        env['PW_USE_COLOR'] = '1'

        for i, cmd in enumerate(self.build_commands, 1):
            _LOG.info('[%d/%d] Starting build: %s', i, num_builds, cmd)

            # Run the build. Put a blank before/after for visual separation.
            print()
            self._current_build = subprocess.Popen(
                ['ninja', '-C', *cmd.args()], env=env)
            returncode = self._current_build.wait()
            print()

            build_ok = (returncode == 0)
            if build_ok:
                level = logging.INFO
                tag = '(OK)'
            else:
                level = logging.ERROR
                tag = '(FAIL)'
            _LOG.log(level, '[%d/%d] Finished build: %s %s', i, num_builds,
                     cmd, tag)
            self.builds_succeeded.append(build_ok)

    # Implementation of DebouncedFunction.cancel()
    def cancel(self):
        if self.restart_on_changes:
            self._current_build.kill()
            return True

        return False

    # Implementation of DebouncedFunction.run()
    def on_complete(self, cancelled=False):
        # First, use the standard logging facilities to report build status.
        if cancelled:
            _LOG.error('Finished; build was interrupted')
        elif all(self.builds_succeeded):
            _LOG.info('Finished; all successful')
        else:
            _LOG.info('Finished; some builds failed')

        # Then, show a more distinct colored banner.
        if not cancelled:
            # Write out build summary table so you can tell which builds passed
            # and which builds failed.
            print()
            print(' .------------------------------------')
            print(' |')
            for (succeeded, cmd) in zip(self.builds_succeeded,
                                        self.build_commands):
                slug = (self.charset.slug_ok
                        if succeeded else self.charset.slug_fail)
                print(f' |   {slug}  {cmd}')
            print(' |')
            print(" '------------------------------------")
        else:
            # Build was interrupted.
            print()
            print(' .------------------------------------')
            print(' |')
            print(' |  ', self.charset.slug_fail, '- interrupted')
            print(' |')
            print(" '------------------------------------")

        # Show a large color banner so it is obvious what the overall result is.
        if all(self.builds_succeeded) and not cancelled:
            print(_COLOR.green(_PASS_MESSAGE))
        else:
            print(_COLOR.red(_FAIL_MESSAGE))

        self.matching_path = None

    # Implementation of DebouncedFunction.on_keyboard_interrupt()
    def on_keyboard_interrupt(self):
        _exit_due_to_interrupt()


_WATCH_PATTERN_DELIMITER = ','
_WATCH_PATTERNS = (
    '*.bloaty',
    '*.c',
    '*.cc',
    '*.cpp',
    '*.cmake',
    'CMakeLists.txt',
    '*.gn',
    '*.gni',
    '*.go',
    '*.h',
    '*.hpp',
    '*.ld',
    '*.md',
    '*.options',
    '*.proto',
    '*.py',
    '*.rst',
)


def add_parser_arguments(parser: argparse.ArgumentParser) -> None:
    """Sets up an argument parser for pw watch."""
    parser.add_argument('--patterns',
                        help=(_WATCH_PATTERN_DELIMITER +
                              '-delimited list of globs to '
                              'watch to trigger recompile'),
                        default=_WATCH_PATTERN_DELIMITER.join(_WATCH_PATTERNS))
    parser.add_argument('--ignore_patterns',
                        dest='ignore_patterns_string',
                        help=(_WATCH_PATTERN_DELIMITER +
                              '-delimited list of globs to '
                              'ignore events from'))

    parser.add_argument('--exclude_list',
                        nargs='+',
                        type=Path,
                        help=('directories to ignore during pw watch'),
                        default=[])
    parser.add_argument('--restart',
                        action='store_true',
                        help='restart an ongoing build if files change')
    parser.add_argument(
        'default_build_targets',
        nargs='*',
        metavar='target',
        default=[],
        help=('Automatically locate a build directory and build these '
              'targets. For example, `host docs` searches for a Ninja '
              'build directory (starting with out/) and builds the '
              '`host` and `docs` targets. To specify one or more '
              'directories, ust the -C / --build_directory option.'))
    parser.add_argument(
        '-C',
        '--build_directory',
        dest='build_directories',
        nargs='+',
        action='append',
        default=[],
        metavar=('directory', 'target'),
        help=('Specify a build directory and optionally targets to '
              'build. `pw watch -C out tgt` is equivalent to `ninja '
              '-C out tgt`'))


def _exit(code):
    # Note: The "proper" way to exit is via observer.stop(), then
    # running a join. However it's slower, so just exit immediately.
    #
    # Additionally, since there are several threads in the watcher, the usual
    # sys.exit approach doesn't work. Instead, run the low level exit which
    # kills all threads.
    os._exit(code)  # pylint: disable=protected-access


def _exit_due_to_interrupt():
    # To keep the log lines aligned with each other in the presence of
    # a '^C' from the keyboard interrupt, add a newline before the log.
    print()
    print()
    _LOG.info('Got Ctrl-C; exiting...')
    _exit(0)


def _exit_due_to_inotify_limit():
    # Show information and suggested commands in OSError: inotify limit reached.
    _LOG.error('Inotify limit reached: run this in your terminal if you '
               'are in Linux to temporarily increase inotify limit.  \n')
    print(
        _COLOR.green('        sudo sysctl fs.inotify.max_user_watches='
                     '$NEW_LIMIT$\n'))
    print('  Change $NEW_LIMIT$ with an integer number, '
          'e.g., 1000 should be enough.')
    _exit(0)


def _exit_due_to_pigweed_not_installed():
    # Show information and suggested commands when pigweed environment variable
    # not found.
    _LOG.error('Environment variable $PW_ROOT not defined or is defined '
               'outside the current directory.')
    _LOG.error('Did you forget to activate the Pigweed environment? '
               'Try source ./activate.sh')
    _LOG.error('Did you forget to install the Pigweed environment? '
               'Try source ./bootstrap.sh')
    _exit(1)


# Go over each directory inside of the current directory.
# If it is not on the path of elements in directories_to_exclude, add
# (directory, True) to subdirectories_to_watch and later recursively call
# Observer() on them.
# Otherwise add (directory, False) to subdirectories_to_watch and later call
# Observer() with recursion=False.
def minimal_watch_directories(to_watch: Path, to_exclude: Iterable[Path]):
    """Determine which subdirectory to watch recursively"""
    try:
        to_watch = Path(to_watch)
    except TypeError:
        assert False, "Please watch one directory at a time."

    # Reformat to_exclude.
    directories_to_exclude: List[Path] = [
        to_watch.joinpath(directory_to_exclude)
        for directory_to_exclude in to_exclude
        if to_watch.joinpath(directory_to_exclude).is_dir()
    ]

    # Split the relative path of directories_to_exclude (compared to to_watch),
    # and generate all parent paths needed to be watched without recursion.
    exclude_dir_parents = {to_watch}
    for directory_to_exclude in directories_to_exclude:
        parts = list(
            Path(directory_to_exclude).relative_to(to_watch).parts)[:-1]
        dir_tmp = to_watch
        for part in parts:
            dir_tmp = Path(dir_tmp, part)
            exclude_dir_parents.add(dir_tmp)

    # Go over all layers of directory. Append those that are the parents of
    # directories_to_exclude to the list with recursion==False, and others
    # with recursion==True.
    for directory in exclude_dir_parents:
        dir_path = Path(directory)
        yield dir_path, False
        for item in Path(directory).iterdir():
            if (item.is_dir() and item not in exclude_dir_parents
                    and item not in directories_to_exclude):
                yield item, True


def gitignore_patterns():
    """Load patterns in pw_root_dir/.gitignore and return as [str]"""
    pw_root_dir = Path(os.environ['PW_ROOT'])

    # Get top level .gitignore entries
    gitignore_path = pw_root_dir / Path('.gitignore')
    if gitignore_path.exists():
        for line in gitignore_path.read_text().splitlines():
            globname = line.strip()
            # If line is empty or a comment.
            if not globname or globname.startswith('#'):
                continue
            yield line


def get_common_excludes() -> List[Path]:
    """Find commonly excluded directories, and return them as a [Path]"""
    exclude_list: List[Path] = []

    # Preset exclude list for Pigweed's upstream directories.
    pw_root_dir = Path(os.environ['PW_ROOT'])
    exclude_list.extend([
        pw_root_dir / ignored_directory for ignored_directory in [
            '.environment',  # Bootstrap-created CIPD and Python venv.
            '.presubmit',  # Presubmit-created CIPD and Python venv.
            '.git',  # Pigweed's git repo.
            '.mypy_cache',  # Python static analyzer.
            '.cargo',  # Rust package manager.
            'out',  # Typical build directory.
        ]
    ])

    # Preset exclude for common downstream project structures.
    #
    # By convention, Pigweed projects use "out" as a build directory, so if
    # watch is invoked outside the Pigweed root, also ignore the local out
    # directory.
    if Path.cwd() != pw_root_dir:
        exclude_list.append(Path('out'))

    # Check for and warn about legacy directories.
    legacy_directories = [
        '.cipd',  # Legacy CIPD location.
        '.python3-venv',  # Legacy Python venv location.
    ]
    found_legacy = False
    for legacy_directory in legacy_directories:
        full_legacy_directory = pw_root_dir / legacy_directory
        if full_legacy_directory.is_dir():
            _LOG.warning('Legacy environment directory found: %s',
                         str(full_legacy_directory))
            exclude_list.append(full_legacy_directory)
            found_legacy = True
    if found_legacy:
        _LOG.warning('Found legacy environment directory(s); these '
                     'should be deleted')

    return exclude_list


def _find_build_dir(default_build_dir: Path = Path('out')) -> Optional[Path]:
    """Searches for a build directory, returning the first it finds."""
    # Give priority to out/, then something under out/.
    if default_build_dir.joinpath('build.ninja').exists():
        return default_build_dir

    for path in default_build_dir.glob('**/build.ninja'):
        return path.parent

    for path in Path.cwd().glob('**/build.ninja'):
        return path.parent

    return None


def watch(default_build_targets: List[str], build_directories: List[str],
          patterns: str, ignore_patterns_string: str, exclude_list: List[Path],
          restart: bool):
    """Watches files and runs Ninja commands when they change."""
    _LOG.info('Starting Pigweed build watcher')

    # Get pigweed directory information from environment variable PW_ROOT.
    if os.environ['PW_ROOT'] is None:
        _exit_due_to_pigweed_not_installed()
    pw_root = Path(os.environ['PW_ROOT']).resolve()
    if Path.cwd().resolve() not in [pw_root, *pw_root.parents]:
        _exit_due_to_pigweed_not_installed()

    # Preset exclude list for pigweed directory.
    exclude_list += get_common_excludes()

    build_commands = [
        BuildCommand(Path(build_dir[0]), tuple(build_dir[1:]))
        for build_dir in build_directories
    ]

    # If no build directory was specified, search the tree for a build.ninja.
    if default_build_targets or not build_directories:
        build_dir = _find_build_dir()

        # Make sure we found something; if not, bail.
        if build_dir is None:
            _die("No build dirs found. Did you forget to run 'gn gen out'?")

        build_commands.append(
            BuildCommand(build_dir, tuple(default_build_targets)))

    # Verify that the build output directories exist.
    for i, build_target in enumerate(build_commands, 1):
        if not build_target.build_dir.is_dir():
            _die("Build directory doesn't exist: %s", build_target)
        else:
            _LOG.info('Will build [%d/%d]: %s', i, len(build_commands),
                      build_target)

    _LOG.debug('Patterns: %s', patterns)

    # Try to make a short display path for the watched directory that has
    # "$HOME" instead of the full home directory. This is nice for users
    # who have deeply nested home directories.
    path_to_log = str(Path().resolve()).replace(str(Path.home()), '$HOME')

    # Ignore the user-specified patterns.
    ignore_patterns = (ignore_patterns_string.split(_WATCH_PATTERN_DELIMITER)
                       if ignore_patterns_string else [])
    # Ignore top level pw_root_dir/.gitignore patterns.
    ignore_patterns += gitignore_patterns()

    ignore_dirs = ['.presubmit', '.python3-env']

    env = pw_cli.env.pigweed_environment()
    if env.PW_EMOJI:
        charset = _EMOJI_CHARSET
    else:
        charset = _ASCII_CHARSET

    event_handler = PigweedBuildWatcher(
        patterns=patterns.split(_WATCH_PATTERN_DELIMITER),
        ignore_patterns=ignore_patterns,
        build_commands=build_commands,
        ignore_dirs=ignore_dirs,
        charset=charset,
        restart=restart,
    )

    try:
        # It can take awhile to configure the filesystem watcher, so have the
        # message reflect that with the "...". Run inside the try: to
        # gracefully handle the user Ctrl-C'ing out during startup.

        _LOG.info('Attaching filesystem watcher to %s/...', path_to_log)

        # Observe changes for all files in the root directory. Whether the
        # directory should be observed recursively or not is determined by the
        # second element in subdirectories_to_watch.
        observers = []
        for path, rec in minimal_watch_directories(Path.cwd(), exclude_list):
            observer = Observer()
            observer.schedule(
                event_handler,
                str(path),
                recursive=rec,
            )
            observer.start()
            observers.append(observer)

        event_handler.debouncer.press('Triggering initial build...')
        for observer in observers:
            while observer.is_alive():
                observer.join(1)

    # Ctrl-C on Unix generates KeyboardInterrupt
    # Ctrl-Z on Windows generates EOFError
    except (KeyboardInterrupt, EOFError):
        _exit_due_to_interrupt()
    except OSError as err:
        if err.args[0] == _ERRNO_INOTIFY_LIMIT_REACHED:
            _exit_due_to_inotify_limit()
        else:
            raise err

    _LOG.critical('Should never get here')
    observer.join()


def main():
    """Watch files for changes and rebuild."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    add_parser_arguments(parser)
    watch(**vars(parser.parse_args()))


if __name__ == '__main__':
    main()
