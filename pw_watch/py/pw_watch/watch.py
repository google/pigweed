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
"""Rebuild every time a file is changed."""

import argparse
from dataclasses import dataclass
import glob
import logging
import os
import pathlib
import shlex
import subprocess
import sys
import threading
from typing import List, NamedTuple, Optional, Sequence, Tuple

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer
from watchdog.utils import has_attribute
from watchdog.utils import unicode_paths

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
def _die(*args):
    _LOG.fatal(*args)
    sys.exit(1)


# pylint: disable=logging-format-interpolation


class WatchCharset(NamedTuple):
    slug_ok: str
    slug_fail: str


_ASCII_CHARSET = WatchCharset(_COLOR.green('OK  '), _COLOR.red('FAIL'))
_EMOJI_CHARSET = WatchCharset('✔️ ', '💥')


@dataclass(frozen=True)
class BuildCommand:
    build_dir: pathlib.Path
    targets: Tuple[str, ...] = ()

    def args(self) -> Tuple[str, ...]:
        return (str(self.build_dir), *self.targets)

    def __str__(self) -> str:
        return shlex.join(self.args())


class PigweedBuildWatcher(FileSystemEventHandler, DebouncedFunction):
    """Process filesystem events and launch builds if necessary."""
    def __init__(
        self,
        patterns: Sequence[str] = (),
        ignore_patterns: Sequence[str] = (),
        case_sensitive: bool = False,
        build_commands: Sequence[BuildCommand] = (),
        ignore_dirs=Optional[List[str]],
        charset: WatchCharset = _ASCII_CHARSET,
    ):
        super(PigweedBuildWatcher, self).__init__()

        self.patterns = patterns
        self.ignore_patterns = ignore_patterns
        self.case_sensitive = case_sensitive
        self.build_commands = build_commands
        self.ignore_dirs = ignore_dirs or []
        self.ignore_dirs.extend(cmd.build_dir for cmd in self.build_commands)
        self.cooldown_finish_time = None
        self.charset: WatchCharset = charset

        self.debouncer = Debouncer(self)

        # Track state of a build. These need to be members instead of locals
        # due to the split between dispatch(), run(), and on_complete().
        self.matching_path = None
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

    def path_matches(self, raw_path):
        """Returns true if path matches according to the watcher patterns"""
        modified_path = pathlib.Path(raw_path).resolve()

        # Check for modifications inside the ignore directories, and skip them.
        # Ideally these events would never hit the watcher, but selectively
        # watching directories at the OS level is not trivial due to limitations
        # of the watchdog module.
        for ignore_dir in self.ignore_dirs:
            resolved_ignore_dir = pathlib.Path(ignore_dir).resolve()
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

    def dispatch(self, event):
        # There isn't any point in triggering builds on new directory creation.
        # It's the creation or modification of files that indicate something
        # meaningful enough changed for a build.
        if event.is_directory:
            return

        # Collect paths of interest from the event.
        paths = []
        if has_attribute(event, 'dest_path'):
            paths.append(unicode_paths.decode(event.dest_path))
        if event.src_path:
            paths.append(unicode_paths.decode(event.src_path))
        for path in paths:
            _LOG.debug('File event: %s', path)

        # Check for matching paths among the one or two in the event.
        matching_path = None
        for path in paths:
            if self.path_matches(path):
                _LOG.debug('Detected event: %s', path)
                matching_path = path
                break

        if matching_path:
            self.handle_matched_event(matching_path)

    def handle_matched_event(self, matching_path):
        if self.matching_path is None:
            self.matching_path = matching_path

        self.debouncer.press('File change detected')

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
        for i, cmd in enumerate(self.build_commands, 1):
            _LOG.info('[%d/%d] Starting build: %s', i, num_builds, cmd)

            # Run the build. Put a blank before/after for visual separation.
            print()
            env = os.environ.copy()
            # Force colors in Pigweed subcommands run through the watcher.
            env['PW_USE_COLOR'] = '1'
            result = subprocess.run(['ninja', '-C', *cmd.args()], env=env)
            print()

            build_ok = (result.returncode == 0)
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
        # TODO: Finish implementing this by supporting cancelling the currently
        # running build. This will require some subprocess shenanigans and
        # so will leave this for later.
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


def add_parser_arguments(parser):
    parser.add_argument('--patterns',
                        help=(_WATCH_PATTERN_DELIMITER +
                              '-delimited list of globs to '
                              'watch to trigger recompile'),
                        default=_WATCH_PATTERN_DELIMITER.join(_WATCH_PATTERNS))
    parser.add_argument('--ignore_patterns',
                        help=(_WATCH_PATTERN_DELIMITER +
                              '-delimited list of globs to '
                              'ignore events from'))

    parser.add_argument('--exclude_list',
                        nargs='+',
                        help=('directories to ignore during pw watch'),
                        default=[])

    parser.add_argument(
        'build_targets',
        nargs='*',
        default=[],
        help=('A Ninja directory to build, followed by specific targets to '
              'build. For example, `out host docs` builds the `host` and '
              '`docs` Ninja targets in the `out` directory. To build '
              'additional directories, use `--build-directory`.'))

    parser.add_argument(
        '--build-directory',
        nargs='+',
        action='append',
        default=[],
        metavar=('dir', 'target'),
        help=('Allows additional build directories to be specified. Uses the '
              'same syntax as `build_targets`.'))


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


def is_subdirectory(child, parent):
    return (pathlib.Path(parent).resolve()
            in pathlib.Path(pathlib.Path(child).resolve()).parents)


# Go over each directory inside of the current directory.
# If it is not on the path of elements in directories_to_exclude, add
# (directory, True) to subdirectories_to_watch and later recursively call
# Observer() on them.
# Otherwise add (directory, False) to subdirectories_to_watch and later call
# Observer() with recursion=False.
def minimal_watch_directories(directory_to_watch, directories_to_exclude):
    """Determine which subdirectory to watch recursively"""
    try:
        cur_dir = pathlib.Path(directory_to_watch)
    except TypeError:
        assert False, "Please watch one directory at a time."
    subdirectories_to_watch = []

    # Reformat directories_to_exclude.
    directories_to_exclude = [
        pathlib.Path(cur_dir, directory_to_exclude)
        for directory_to_exclude in directories_to_exclude
        if pathlib.Path(cur_dir, directory_to_exclude).is_dir()
    ]

    # Split the relative path of directories_to_exclude (compared to
    # directory_to_watch), and generate all parent paths needed to be
    # watched without recursion.
    exclude_dir_parents = {cur_dir}
    for directory_to_exclude in directories_to_exclude:
        parts = list(
            pathlib.Path(directory_to_exclude).relative_to(cur_dir).parts)[:-1]
        dir_tmp = cur_dir
        for part in parts:
            dir_tmp = pathlib.Path(dir_tmp, part)
            exclude_dir_parents.add(dir_tmp)

    # Go over all layers of directory. Append those that are the parents of
    # directories_to_exclude to the list with recursion==False, and others
    # with recursion==True.
    for directory in exclude_dir_parents:
        dir_path = pathlib.Path(directory)
        subdirectories_to_watch.append((dir_path, False))
        for item in pathlib.Path(directory).iterdir():
            if (item.is_dir() and item not in exclude_dir_parents
                    and item not in directories_to_exclude):
                subdirectories_to_watch.append((item, True))

    return subdirectories_to_watch


def get_exclude_list(exclude_list):
    # Preset exclude list for pigweed directory.
    pigweed_exclude_list = [
        pathlib.Path(os.environ['PW_ROOT'], x)
        for x in ['.cipd', '.git', 'out', '.python3-env', '.presubmit']
    ]
    return exclude_list + pigweed_exclude_list


def watch(build_targets=None,
          build_directory=None,
          patterns=None,
          ignore_patterns=None,
          exclude_list=None):
    """TODO(keir) docstring"""

    _LOG.info('Starting Pigweed build watcher')

    # Get pigweed directory information from environment variable PW_ROOT.
    if os.environ['PW_ROOT'] is None:
        _exit_due_to_pigweed_not_installed()
    path_of_pigweed = pathlib.Path(os.environ['PW_ROOT'])
    cur_dir = pathlib.Path(os.getcwd())
    if (not (is_subdirectory(path_of_pigweed, cur_dir)
             or path_of_pigweed == cur_dir)):
        _exit_due_to_pigweed_not_installed()

    # Preset exclude list for pigweed directory.
    exclude_list = get_exclude_list(exclude_list)

    subdirectories_to_watch \
        = minimal_watch_directories(cur_dir, exclude_list)

    # If no build directory was specified, search the tree for GN build
    # directories and try to build them all. In the future this may cause
    # slow startup, but for now this is fast enough.
    build_commands = []
    if not build_targets and not build_directory:
        _LOG.info('Searching for GN build dirs...')
        gn_args_files = []
        if os.path.isfile('out/args.gn'):
            gn_args_files += ['out/args.gn']
        gn_args_files += glob.glob('out/*/args.gn')

        for gn_args_file in gn_args_files:
            gn_build_dir = pathlib.Path(gn_args_file).parent
            gn_build_dir = gn_build_dir.resolve().relative_to(cur_dir)
            if gn_build_dir.is_dir():
                build_commands.append(BuildCommand(gn_build_dir))
    else:
        if build_targets:
            build_directory.append(build_targets)
        # Reformat the directory of build commands to be relative to the
        # currently directory.
        for build_target in build_directory:
            build_commands.append(
                BuildCommand(pathlib.Path(build_target[0]),
                             tuple(build_target[1:])))

    # Make sure we found something; if not, bail.
    if not build_commands:
        _die("No build dirs found. Did you forget to 'gn gen out'?")

    # Verify that the build output directories exist.
    for i, build_target in enumerate(build_commands, 1):
        if not build_target.build_dir.is_dir():
            _die("Build directory doesn't exist: %s", build_target)
        else:
            _LOG.info('Will build [%d/%d]: %s', i, len(build_commands),
                      build_target)

    _LOG.debug('Patterns: %s', patterns)

    path_of_directory_to_watch = '.'

    # Try to make a short display path for the watched directory that has
    # "$HOME" instead of the full home directory. This is nice for users
    # who have deeply nested home directories.
    path_to_log = pathlib.Path(path_of_directory_to_watch).resolve()
    try:
        path_to_log = path_to_log.relative_to(pathlib.Path.home())
        path_to_log = f'$HOME/{path_to_log}'
    except ValueError:
        # The directory is somewhere other than inside the users home.
        path_to_log = path_of_directory_to_watch

    # Ignore the user-specified patterns.
    ignore_patterns = (ignore_patterns.split(_WATCH_PATTERN_DELIMITER)
                       if ignore_patterns else [])

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
        for directory, rec in subdirectories_to_watch:
            observer = Observer()
            observer.schedule(
                event_handler,
                str(directory),
                recursive=rec,
            )
            observer.start()
            observers.append(observer)

        event_handler.debouncer.press('Triggering initial build...')
        for observer in observers:
            while observer.isAlive():
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
    parser = argparse.ArgumentParser(description=main.__doc__)
    add_parser_arguments(parser)
    watch(**vars(parser.parse_args()))


if __name__ == '__main__':
    main()
