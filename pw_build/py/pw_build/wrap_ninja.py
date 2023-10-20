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
import enum
import json
import os
import re
import signal
import subprocess
import sys
import threading
import time
from typing import Any, Dict, IO, List, Tuple, Optional

if sys.platform != 'win32':
    import pty

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


class SignalFlag:
    """Helper class for intercepting signals and setting a flag to allow
    gracefully handling them.

    Attributes:
        signaled: Initially False, becomes True when the process receives a
          SIGTERM.
    """

    def __init__(self) -> None:
        """Initializes the signal handler."""
        self.signaled = False
        signal.signal(signal.SIGTERM, self._signal_handler)

    # pylint: disable=unused-argument
    def _signal_handler(self, signal_number: int, stack_frame: Any) -> None:
        """Sets the signaled flag, called when signaled."""
        self.signaled = True


class ConsoleRenderer:
    """Helper class for rendering the TUI using terminal control codes.

    This class has two types of output. First, permanently printed lines
    (with `print_line` method) behave like normal terminal output and stay in
    the terminal once printed. Second, "temporary" printed lines follow the
    most recently printed permanent line, and get rewritten on each call
    to `render`.

    Attributes:
        smart_terminal: If true, will print a rich TUI using terminal control
            codes. Otherwise, this class won't print temporary lines, and
            permanent lines will be printed without any special control codes.
            Defaults to true if stdout is connected to a TTY.
    """

    def __init__(self) -> None:
        """Initialize the console renderer."""
        self._queued_lines: List[str] = []
        self._temporary_lines: List[str] = []
        self._previous_line_count = 0

        term = os.environ.get('TERM')
        self.smart_terminal = term and (term != 'dumb') and sys.stdout.isatty()

    def print_line(self, line: str) -> None:
        """Queue a permanent line for printing during the next render."""
        self._queued_lines.append(line)

    def set_temporary_lines(self, lines: List[str]) -> None:
        """Set the temporary lines to be displayed during the next render."""
        self._temporary_lines = lines[:]

    def render(self) -> None:
        """Render the current state of the renderer."""

        # If we can't use terminal codes, print out permanent lines and exit.
        if not self.smart_terminal:
            for line in self._queued_lines:
                print(line)
            self._queued_lines.clear()
            sys.stdout.flush()
            return

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
        end_time: The time that the Ninja action finished, or None if it is
            still running.
    """

    name: str
    jobs: int = 0
    start_time: float = dataclasses.field(default_factory=time.time)
    end_time: Optional[float] = None


class NinjaEventKind(enum.Enum):
    """The kind of Ninja event."""

    ACTION_STARTED = 1
    ACTION_FINISHED = 2
    ACTION_LOG = 3


@dataclasses.dataclass
class NinjaEvent:
    """An event from the Ninja build.

    Attributes:
        kind: The kind of event.
        action: The action this event relates to, if any.
        message: The log message associated with this event, if it an
            'ACTION_LOG' event.
        num_started: The number of started actions when this event occurred.
        num_finished: The number of finished actions when this event occurred.
        num_total: The total number of actions when this event occurred.
    """

    kind: NinjaEventKind
    action: Optional[NinjaAction] = None
    log_message: Optional[str] = None
    num_started: int = 0
    num_finished: int = 0
    num_total: int = 0


class Ninja:
    """Wrapper around a Ninja subprocess.

    Parses the Ninja output to maintain a representation of the build graph.

    Attributes:
        num_started: The number of started actions.
        num_finished: The number of finished actions.
        num_total: The (estimated) number of total actions in the build.
        running_actions: The currently-running actions, keyed by name.
        actions: A list of all actions, running and completed.
        last_action_completed: The last action that Ninja finished building.
        events: The events that have occured in the Ninja build.
        exited: Whether the Ninja subprocess has exited.
        lock: The lock guarding the rest of the attributes.
        process: The Python subprocess running Ninja.
        start_time: The time that the Ninja build started.
    """

    num_started: int
    num_finished: int
    num_total: int
    actions: List[NinjaAction]
    running_actions: Dict[str, NinjaAction]
    last_action_completed: Optional[NinjaAction]
    events: List[NinjaEvent]
    exited: bool
    lock: threading.Lock
    process: subprocess.Popen
    start_time: float

    def __init__(self, args: List[str]) -> None:
        if sys.platform == 'win32':
            raise RuntimeError('Ninja wrapper does not support Windows.')

        self.num_started = 0
        self.num_finished = 0
        self.num_total = 0
        self.actions = []
        self.running_actions = {}
        self.last_action_completed = None
        self.events = []
        self.exited = False
        self.lock = threading.Lock()

        # Launch ninja and configure pseudo-tty.
        # pylint: disable-next=no-member,undefined-variable,used-before-assignment
        ptty_parent, ptty_child = pty.openpty()  # type: ignore
        ptty_file = os.fdopen(ptty_parent, 'r')
        env = dict(os.environ)
        env['NINJA_STATUS'] = _NINJA_STATUS
        if 'TERM' not in env:
            env['TERM'] = 'vt100'
        command = ['ninja'] + args
        self.process = subprocess.Popen(
            command,
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=ptty_child,
            stderr=ptty_child,
        )
        os.close(ptty_child)
        self.start_time = time.time()

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
                if char == '':
                    # End of file.
                    break

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
            pass

        self._process_output(''.join(buffer))
        self.exited = True

    def _process_output(self, line: str) -> None:
        """Process a line of output from Ninja, updating the internal state."""
        with self.lock:
            if match := re.match(r'^@@!!@@(\d+),(\d+),(\d+)>(.*)', line):
                actions_started = int(match.group(1))
                actions_finished = int(match.group(2))
                actions_total = int(match.group(3))
                name = match.group(4)

                # Sometimes Ninja delimits lines without \r, which prevents
                # _monitor_thread from stripping out the final control code,
                # so just remove it here if it's present.
                if name.endswith('\x1B[K'):
                    name = name[:-3]

                did_start = actions_started > self.num_started
                did_finish = actions_finished > self.num_finished
                self.num_started = actions_started
                self.num_finished = actions_finished
                self.num_total = actions_total

                # Special case: first action in a new graph.
                # This is important for GN's "Regenerating ninja files" action.
                if actions_started == 1 and actions_finished == 0:
                    for action in self.running_actions.values():
                        action.end_time = time.time()
                        self._add_event(
                            NinjaEvent(NinjaEventKind.ACTION_FINISHED, action)
                        )
                    self.running_actions = {}
                    self.last_action_completed = None

                if did_start:
                    action = self.running_actions.setdefault(
                        name, NinjaAction(name)
                    )
                    if action.jobs == 0:
                        self.actions.append(action)
                        self._add_event(
                            NinjaEvent(NinjaEventKind.ACTION_STARTED, action)
                        )
                    action.jobs += 1

                if did_finish and name in self.running_actions:
                    action = self.running_actions[name]
                    action.jobs -= 1
                    if action.jobs <= 0:
                        self.running_actions.pop(name)
                        self.last_action_completed = action
                        action.end_time = time.time()
                        self._add_event(
                            NinjaEvent(NinjaEventKind.ACTION_FINISHED, action)
                        )
            else:
                context_action = None
                if not line.startswith('ninja: '):
                    context_action = self.last_action_completed
                self._add_event(
                    NinjaEvent(
                        NinjaEventKind.ACTION_LOG,
                        action=context_action,
                        log_message=line,
                    )
                )

    def _add_event(self, event: NinjaEvent) -> None:
        """Add a new event to the event queue."""
        event.num_started = self.num_started
        event.num_finished = self.num_finished
        event.num_total = self.num_total
        self.events.append(event)

    def write_trace(self, file: IO[str]) -> None:
        """Write a Chromium trace_event-formatted trace to a file."""
        now = time.time()
        threads: List[float] = []
        events = []
        actions = sorted(
            self.actions, key=lambda x: x.end_time or now, reverse=True
        )
        for action in actions:
            # If this action hasn't completed, fake the end time.
            end_time = action.end_time or now

            # Allocate a "thread id" for the action. We look at threads in
            # reverse end time and fill in threads from the end to get a
            # better-looking trace.
            for tid, busy_time in enumerate(threads):
                if busy_time >= end_time:
                    threads[tid] = action.start_time
                    break
            else:
                tid = len(threads)
                threads.append(action.start_time)

            events.append(
                {
                    'name': action.name,
                    'cat': 'actions',
                    'ph': 'X',
                    'ts': action.start_time * 1000000,
                    'dur': (end_time - action.start_time) * 1000000,
                    'pid': 0,
                    'tid': tid,
                    'args': {},
                }
            )
        json.dump(events, file)


class UI:
    """Class to handle UI state and rendering."""

    def __init__(self, ninja: Ninja, args: argparse.Namespace) -> None:
        self._ninja = ninja
        self._args = args
        self._renderer = ConsoleRenderer()
        self._last_log_action: Optional[NinjaAction] = None

    def _get_status_display(self) -> List[str]:
        """Generates the status display. Must be called under the Ninja lock."""
        actions = sorted(
            self._ninja.running_actions.values(), key=lambda x: x.start_time
        )
        now = time.time()
        total_elapsed = _format_duration(now - self._ninja.start_time)
        lines = [
            f'[{total_elapsed: >5}] '
            f'Building [{self._ninja.num_finished}/{self._ninja.num_total}] ...'
        ]
        for action in actions[: self._args.ui_max_actions]:
            elapsed = _format_duration(now - action.start_time)
            lines.append(f'  [{elapsed: >5}] {action.name}')
        if len(actions) > self._args.ui_max_actions:
            remaining = len(actions) - self._args.ui_max_actions
            lines.append(f'  ... and {remaining} more')
        return lines

    def _process_event(self, event: NinjaEvent) -> None:
        """Processes a Ninja Event. Must be called under the Ninja lock."""
        show_started = self._args.log_actions
        show_ended = self._args.log_actions or not self._renderer.smart_terminal

        if event.kind == NinjaEventKind.ACTION_LOG:
            if event.action and (event.action != self._last_log_action):
                self._renderer.print_line(f'[{event.action.name}]')
            self._last_log_action = event.action
            assert event.log_message is not None
            self._renderer.print_line(event.log_message)

        if event.kind == NinjaEventKind.ACTION_STARTED and show_started:
            assert event.action
            self._renderer.print_line(
                f'[{event.num_finished}/{event.num_total}] '
                f'Started  [{event.action.name}]'
            )

        if event.kind == NinjaEventKind.ACTION_FINISHED and show_ended:
            assert event.action and event.action.end_time is not None
            duration = _format_duration(
                event.action.end_time - event.action.start_time
            )
            self._renderer.print_line(
                f'[{event.num_finished}/{event.num_total}] '
                f'Finished [{event.action.name}] ({duration})'
            )

    def update(self) -> None:
        """Updates and re-renders the UI."""
        with self._ninja.lock:
            for event in self._ninja.events:
                self._process_event(event)
            self._ninja.events = []

            self._renderer.set_temporary_lines(self._get_status_display())

        self._renderer.render()

    def print_summary(self) -> None:
        """Prints the summary line at the end of the build."""
        total_time = _format_duration(time.time() - self._ninja.start_time)
        num_finished = self._ninja.num_finished
        num_total = self._ninja.num_total
        self._renderer.print_line(
            f'Built {num_finished}/{num_total} targets in {total_time}.'
        )
        self._renderer.set_temporary_lines([])
        self._renderer.render()

    def dump_running_actions(self) -> None:
        """Prints a list of actions that are currently running."""
        actions = sorted(
            self._ninja.running_actions.values(), key=lambda x: x.start_time
        )
        now = time.time()
        self._renderer.print_line(f'{len(actions)} running actions:')
        for action in actions:
            elapsed = _format_duration(now - action.start_time)
            self._renderer.print_line(f'  [{elapsed: >5}] {action.name}')
        self._renderer.set_temporary_lines([])
        self._renderer.render()

    def print_line(self, line: str) -> None:
        """Print a line to the interface output."""
        self._renderer.print_line(line)


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
    parser.add_argument(
        '--log-actions',
        help='whether to log when actions start and finish',
        action='store_true',
    )
    parser.add_argument(
        '--write-trace',
        help='write a Chromium trace_event-style trace to the given file',
        type=argparse.FileType('w'),
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

    signal_flag = SignalFlag()
    ninja = Ninja(ninja_args)
    interface = UI(ninja, args)

    while True:
        interface.update()
        if signal_flag.signaled:
            interface.print_line('Got SIGTERM, exiting...')
            ninja.process.terminate()
            break
        if ninja.exited:
            break
        time.sleep(args.ui_update_rate)

    # Finish up the build output.
    ninja.process.wait()
    interface.update()
    interface.print_summary()

    if args.write_trace is not None:
        ninja.write_trace(args.write_trace)
        print(f'Wrote trace to {args.write_trace.name}')

    if signal_flag.signaled:
        interface.dump_running_actions()

    return ninja.process.returncode


if __name__ == '__main__':
    sys.exit(main())
