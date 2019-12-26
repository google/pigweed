# Copyright 2019 The Pigweed Authors
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
"""Rebuild every time a file is chanegd."""

import argparse
import enum
import glob
import logging
import os
import pathlib
import subprocess
import sys
import time

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer
from watchdog.utils import has_attribute
from watchdog.utils import unicode_paths

import pw_cli.color
import pw_cli.plugins

_COLOR = pw_cli.color.colors()
_LOG = logging.getLogger(__name__)

_BUILD_MESSAGE = """
  ██████╗ ██╗   ██╗██╗██╗     ██████╗
  ██╔══██╗██║   ██║██║██║     ██╔══██╗
  ██████╔╝██║   ██║██║██║     ██║  ██║
  ██╔══██╗██║   ██║██║██║     ██║  ██║
  ██████╔╝╚██████╔╝██║███████╗██████╔╝
  ╚═════╝  ╚═════╝ ╚═╝╚══════╝╚═════╝
"""

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


class _State(enum.Enum):
    WAITING_FOR_FILE_CHANGE_EVENT = 1
    COOLDOWN_IGNORING_EVENTS = 2


# TODO(keir): Figure out a better strategy for exiting. The problem with the
# watcher is that doing a "clean exit" is slow. However, by directly exiting,
# we remove the possibility of the wrapper script doing anything on exit.
def _die(*args):
    _LOG.fatal(*args)
    sys.exit(1)


# pylint: disable=logging-format-interpolation


class PigweedBuildWatcher(FileSystemEventHandler):
    """Process filesystem events and launch builds if necessary."""
    def __init__(self,
                 patterns=None,
                 ignore_patterns=None,
                 case_sensitive=False,
                 build_dirs=None,
                 ignore_dirs=None):
        super().__init__()

        self.patterns = patterns
        self.ignore_patterns = ignore_patterns
        self.case_sensitive = case_sensitive
        self.state = _State.WAITING_FOR_FILE_CHANGE_EVENT
        self.build_dirs = build_dirs or []
        self.ignore_dirs = (ignore_dirs or []) + self.build_dirs
        self.cooldown_finish_time = None

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

    def run_builds(self, matching_path):
        """Run all the builds in serial and capture pass/fail for each."""

        # Clear the screen and show a banner indicating the build is starting.
        print("\033c", end="")  # TODO(pwbug/38): Not Windows compatible.
        print(_COLOR.magenta(_BUILD_MESSAGE))
        _LOG.info('Change detected: %s', matching_path)

        builds_succeeded = []
        num_builds = len(self.build_dirs)
        _LOG.info(f'Starting build with {num_builds} directories')
        for i, build_dir in enumerate(self.build_dirs, 1):
            _LOG.info(f'[{i}/{num_builds}] Starting build: {build_dir}')

            # Run the build. Put a blank before/after for visual separation.
            print()
            result = subprocess.run(['ninja', '-C', build_dir])
            print()

            build_ok = (result.returncode == 0)
            if build_ok:
                level = logging.INFO
                tag = '(OK)'
            else:
                level = logging.ERROR
                tag = '(FAIL)'
            _LOG.log(level,
                     f'[{i}/{num_builds}] Finished build: {build_dir} {tag}')
            builds_succeeded.append(build_ok)

        if all(builds_succeeded):
            _LOG.info('Finished; all successful.')
        else:
            _LOG.info('Finished; some builds failed.')

        # Write out build summary table so you can tell which builds passed
        # and which builds failed.
        print()
        print(' .------------------------------------')
        print(' |')
        for (succeeded, build_dir) in zip(builds_succeeded, self.build_dirs):
            if succeeded:
                slug = _COLOR.green('OK  ')
            else:
                slug = _COLOR.red('FAIL')

            print(f' |   {slug}  {build_dir}')
        print(' |')
        print(" '------------------------------------")

        # Show a large color banner so it is obvious what the overall result is.
        if all(builds_succeeded):
            print(_COLOR.green(_PASS_MESSAGE))
        else:
            print(_COLOR.red(_FAIL_MESSAGE))

    def handle_matched_event(self, matching_path):
        if self.state == _State.WAITING_FOR_FILE_CHANGE_EVENT:
            self.run_builds(matching_path)

            # Don't set the cooldown end time until after the build.
            self.state = _State.COOLDOWN_IGNORING_EVENTS
            _LOG.debug('State: WAITING -> COOLDOWN (file change trigger)')

            # 500ms is enough to allow the spurious events to get ignored.
            self.cooldown_finish_time = time.time() + 0.5

        elif self.state == _State.COOLDOWN_IGNORING_EVENTS:
            if time.time() < self.cooldown_finish_time:
                _LOG.debug('Skipping event; cooling down...')
            else:
                _LOG.debug('State: COOLDOWN -> WAITING (cooldown expired)')
                self.state = _State.WAITING_FOR_FILE_CHANGE_EVENT
                self.handle_matched_event(matching_path)  # Retrigger.


_WATCH_PATTERN_DELIMITER = ','
_WATCH_PATTERNS = (
    '*.bloaty',
    '*.c',
    '*.cc',
    '*.gn',
    '*.gni',
    '*.go',
    '*.h',
    '*.ld',
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
    parser.add_argument(
        '--build_dir',
        help=('Ninja directory to build. Can be specified '
              'multiple times to build multiple configurations'),
        action='append')


def watch(build_dir='', patterns=None, ignore_patterns=None):
    """TODO(keir) docstring"""

    _LOG.info('Starting Pigweed build watcher')

    # If no build directory was specified, search the tree for GN build
    # directories and try to build them all. In the future this may cause
    # slow startup, but for now this is fast enough.
    build_dirs = build_dir
    if not build_dirs:
        build_dirs = []
        _LOG.info('Searching for GN build dirs...')
        gn_args_files = glob.glob('**/args.gn', recursive=True)
        for gn_args_file in gn_args_files:
            gn_build_dir = pathlib.Path(gn_args_file).parent
            if gn_build_dir.is_dir():
                build_dirs.append(str(gn_build_dir))

    # Make sure we found something; if not, bail.
    if not build_dirs:
        _die("No build dirs found. Did you forget to 'gn gen out'?")

    # Verify that the build output directories exist.
    # pylint: disable=redefined-argument-from-local
    for i, build_dir in enumerate(build_dirs, 1):
        if not os.path.isdir(build_dir):
            _die("Build directory doesn't exist: %s", build_dir)
        else:
            _LOG.info(f'Will build [{i}/{len(build_dirs)}]: {build_dir}')
    # pylint: enable=redefined-argument-from-local

    _LOG.debug('Patterns: %s', patterns)

    # TODO(keir): Change the watcher to selectively watch some
    # subdirectories, rather than watching everything under a single path.
    #
    # The problem with the current approach is that Ninja's building
    # triggers many events, which are needlessly sent to this script.
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

    event_handler = PigweedBuildWatcher(
        patterns=patterns.split(_WATCH_PATTERN_DELIMITER),
        ignore_patterns=ignore_patterns,
        build_dirs=build_dirs,
        ignore_dirs=ignore_dirs)

    observer = Observer()
    observer.schedule(
        event_handler,
        path_of_directory_to_watch,
        recursive=True,
    )
    observer.start()

    _LOG.info('Directory to watch: %s', path_to_log)
    _LOG.info('Watching for file changes. Ctrl-C exits.')

    # Make a nice non-logging banner to motivate the user.
    print()
    print(_COLOR.green('  WATCHER IS READY: GO WRITE SOME CODE!'))
    print()

    try:
        while observer.isAlive():
            observer.join(1)
    except KeyboardInterrupt:
        # To keep the log lines aligned with each other in the presence of
        # a '^C' from the keyboard interrupt, add a newline before the log.
        print()
        _LOG.info('Got Ctrl-C; exiting...')

        # Note: The "proper" way to exit is via observer.stop(), then
        # running a join. However it's slower, so just exit immediately.
        sys.exit(0)

    observer.join()


pw_cli.plugins.register(
    name='watch',
    short_help='Watch files for changes',
    define_args_function=add_parser_arguments,
    command_function=watch,
)


def main():
    parser = argparse.ArgumentParser(description='Watch for changes')
    add_parser_arguments(parser)
    args = parser.parse_args()
    watch(**vars(args))


if __name__ == '__main__':
    main()
