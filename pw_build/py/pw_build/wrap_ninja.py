#!/usr/bin/env python3
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
"""Wrapper for Ninja that adds a real-time status interface."""

import argparse
import dataclasses
import os
import pty
import re
import subprocess
import sys
import threading
import time
from typing import Dict, IO, List, Tuple, Optional

# The status formatting string for Ninja to use. Includes a sentinel prefix.
_NINJA_STATUS = '@@!!@@%s,%f,%t>'

# "ANSI" terminal codes for controlling terminal output. This are more-or-less
# standardized. See https://en.wikipedia.org/wiki/ANSI_escape_code for a more
# comprehensive list.
# Terminal code for clearing the entire current line.
_TERM_CLEAR_LINE_FULL: str = '\x1B[2K'
# Terminal code for clearing from the cursor to the end of the current line.
_TERM_CLEAR_LINE_TO_END: str = '\x1B[0K'
# Terminal code for moving the cursor to the previous line.
_TERM_MOVE_PREVIOUS_LINE: str = '\x1B[1A'
# Terminal code for stopping line wrapping.
_TERM_DISABLE_WRAP: str = '\x1B[?7l'
# Terminal code for enabling line wrapping.
_TERM_ENABLE_WRAP: str = '\x1B[?7h'


class ConsoleRenderer:
    """Helper class for rendering the TUI using terminal control codes.

    This class has two types of output. First, permanently printed lines
    (with `print_line` method) behave like normal terminal output and stay in
    the terminal once printed. Second, "temporary" printed lines follow the
    most recently printed permanent line, and get rewritten on each call
    to `render`.
    """

    def __init__(self) -> None:
        """Initialize the console renderer."""
        self._queued_lines: List[str] = []
        self._temporary_lines: List[str] = []
        self._previous_line_count = 0

    def print_line(self, line: str) -> None:
        """Queue a permanent line for printing during the next render."""
        self._queued_lines.append(line)

    def set_temporary_lines(self, lines: List[str]) -> None:
        """Set the temporary lines to be displayed during the next render."""
        self._temporary_lines = lines[:]

    def render(self) -> None:
        """Render the current state of the renderer."""

        # Go back to the end of the last permanent lines.
        for _ in range(self._previous_line_count):
            print(_TERM_MOVE_PREVIOUS_LINE, end='')

        # Print any new permanent lines.
        for line in self._queued_lines:
            print(line, end='')
            print(_TERM_CLEAR_LINE_TO_END)

        # Print the new temporary lines.
        print(_TERM_DISABLE_WRAP, end='')
        for line in self._temporary_lines:
            print(_TERM_CLEAR_LINE_FULL, end='')
            print(line, end='')
            print(_TERM_CLEAR_LINE_TO_END)
        print(_TERM_ENABLE_WRAP, end='')

        # Clear any leftover temporary lines from the previous render.
        lines_to_clear = self._previous_line_count - len(self._temporary_lines)
        for _ in range(lines_to_clear):
            print(_TERM_CLEAR_LINE_FULL)
        for _ in range(lines_to_clear):
            print(_TERM_MOVE_PREVIOUS_LINE, end='')

        # Flush and update state.
        sys.stdout.flush()
        self._previous_line_count = len(self._temporary_lines)
        self._queued_lines.clear()


def _format_duration(duration: float) -> str:
    """Format a duration (in seconds) as a string."""
    if duration >= 60:
        seconds = int(duration % 60)
        minutes = int(duration / 60)
        return f'{minutes}m{seconds:02}s'
    return f'{duration:.1f}s'


@dataclasses.dataclass
class NinjaAction:
    """A Ninja action: a task that needs to run and complete.

    In Ninja, this is referred to as an "edge".

    Attributes:
        name: The name of the Ninja action.
        jobs: The number of running jobs for the action. Some Ninja actions
            have multiple sub-actions that have the same name.
        start_time: The time that the Ninja action started. This is based on
            time.time, not Ninja's own stopwatch.
    """

    name: str
    jobs: int = 0
    start_time: float = dataclasses.field(default_factory=time.time)


class Ninja:
    """Wrapper around a Ninja subprocess.

    Parses the Ninja output to maintain a representation of the build graph.

    Attributes:
        num_started: The number of started actions.
        num_finished: The number of finished actions.
        num_total: The (estimated) number of total actions in the build.
        actions: The currently-running actions, keyed by the action name.
        last_action_completed: The last action that Ninja finished building.
        log_lines: The (non-status) lines that have been printed by Ninja. These
            are unstructured messages from Ninja or output from completed
            (and potentially failed) actions.
        exited: Whether the Ninja subprocess has exited.
        lock: The lock guarding the rest of the attributes.
        process: The Python subprocess running Ninja.
    """

    num_started: int
    num_finished: int
    num_total: int
    actions: Dict[str, NinjaAction]
    last_action_completed: Optional[NinjaAction]
    log_lines: List[Tuple[Optional[NinjaAction], str]]
    exited: bool
    lock: threading.Lock
    process: subprocess.Popen

    def __init__(self, args: List[str]) -> None:
        if sys.platform == 'win32':
            raise RuntimeError('Ninja wrapper does not support Windows.')

        self.num_started = 0
        self.num_finished = 0
        self.num_total = 0
        self.actions = {}
        self.last_action_completed = None
        self.log_lines = []
        self.exited = False
        self.lock = threading.Lock()

        # Launch ninja and configure pseudo-tty.
        ptty_parent, ptty_child = pty.openpty()  # pylint: disable=no-member
        ptty_file = os.fdopen(ptty_parent, 'r')
        env = dict(os.environ)
        env['NINJA_STATUS'] = _NINJA_STATUS
        command = ['ninja'] + args
        self.process = subprocess.Popen(
            command,
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=ptty_child,
            stderr=subprocess.DEVNULL,
        )
        os.close(ptty_child)

        # Start the output monitor thread.
        thread = threading.Thread(
            target=self._monitor_thread, args=(ptty_file,)
        )
        thread.daemon = True  # Don't keep the process alive.
        thread.start()

    def _monitor_thread(self, file: IO[str]) -> None:
        """Monitor the Ninja output. Run in a separate thread."""
        # A Ninja status line starts with "\r" and ends with "\x1B[K". This
        # tracks whether we're currently in a status line.
        ninja_status = False
        buffer: List[str] = []
        try:
            while True:
                char = file.read(1)
                if char == '\r':
                    ninja_status = True
                    continue
                if char == '\n':
                    self._process_output(''.join(buffer))
                    buffer = []
                    continue

                # Look for the end of a status line, ignoring partial matches.
                if char == '\x1B' and ninja_status:
                    char = file.read(1)
                    if char == '[':
                        char = file.read(1)
                        if char == 'K':
                            self._process_output(''.join(buffer))
                            buffer = []
                            ninja_status = False
                            continue
                        buffer.append('\x1B[')
                    else:
                        buffer.append('\x1B')

                buffer.append(char)
        except OSError:
            self.exited = True

    def _process_output(self, line: str) -> None:
        """Process a line of output from Ninja, updating the internal state."""
        with self.lock:
            if match := re.match(r'^@@!!@@(\d+),(\d+),(\d+)>(.*)', line):
                actions_started = int(match.group(1))
                actions_finished = int(match.group(2))
                actions_total = int(match.group(3))
                name = match.group(4)

                did_start = actions_started > self.num_started
                did_finish = actions_finished > self.num_finished
                self.num_started = actions_started
                self.num_finished = actions_finished
                self.num_total = actions_total

                # Special case: first action in a new graph.
                # This is important for GN's "Regenerating ninja files" action.
                if actions_started == 1 and actions_finished == 0:
                    self.actions = {}
                    self.last_action_completed = None

                if did_start:
                    action = self.actions.setdefault(name, NinjaAction(name))
                    action.jobs += 1

                if did_finish and name in self.actions:
                    action = self.actions[name]
                    action.jobs -= 1
                    if action.jobs <= 0:
                        self.actions.pop(name)
                        self.last_action_completed = action
            else:
                context_action = None
                if not line.startswith('ninja: '):
                    context_action = self.last_action_completed
                self.log_lines.append((context_action, line))


def _parse_args() -> Tuple[argparse.Namespace, List[str]]:
    """Parses CLI arguments.

    Returns:
        The tuple containing the parsed arguments and the remaining unparsed
        arguments to be passed to Ninja.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--ui-update-rate',
        help='update rate of the UI (in seconds)',
        type=float,
        default=0.1,
    )
    parser.add_argument(
        '--ui-max-actions',
        help='maximum build actions to display at once',
        type=int,
        default=8,
    )
    return parser.parse_known_args()


def main() -> int:
    """Main entrypoint for script."""
    args, ninja_args = _parse_args()

    if sys.platform == 'win32':
        print(
            'WARNING: pw-wrap-ninja does not support Windows. '
            'Running ninja directly.'
        )
        process = subprocess.run(['ninja'] + ninja_args)
        return process.returncode

    ninja = Ninja(ninja_args)
    start_time = time.time()
    renderer = ConsoleRenderer()
    last_action_logged = None

    while True:
        with ninja.lock:
            # Print non-status lines logged.
            for action, line in ninja.log_lines:
                if action != last_action_logged and action is not None:
                    renderer.print_line(f'[{action.name}]')
                last_action_logged = action
                renderer.print_line(line)
            ninja.log_lines = []

            # Generate status display.
            actions = sorted(ninja.actions.values(), key=lambda x: x.start_time)
            now = time.time()
            total_elapsed = _format_duration(now - start_time)
            lines = [
                f'[{total_elapsed: >5}] '
                f'Building [{ninja.num_finished}/{ninja.num_total}] ...'
            ]
            for action in actions[: args.ui_max_actions]:
                elapsed = _format_duration(now - action.start_time)
                lines.append(f'  [{elapsed: >5}] {action.name}')
            if len(actions) > args.ui_max_actions:
                remaining = len(actions) - args.ui_max_actions
                lines.append(f'  ... and {remaining} more')

            if ninja.exited:
                break

        renderer.set_temporary_lines(lines)
        renderer.render()
        time.sleep(args.ui_update_rate)

    # Output the final summary build line.
    ninja.process.wait()
    total_time = _format_duration(time.time() - start_time)
    renderer.print_line(
        f'Built {ninja.num_finished}/{ninja.num_total} targets in {total_time}.'
    )
    renderer.set_temporary_lines([])
    renderer.render()
    return ninja.process.returncode


if __name__ == '__main__':
    sys.exit(main())
