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
"""Run an interruptable, cancellable function after debouncing run requests"""

import enum
import logging
import threading
from abc import ABC, abstractmethod

_LOG = logging.getLogger(__name__)


class DebouncedFunction(ABC):
    """Function to be run by Debouncer"""
    @abstractmethod
    def run(self):
        """Run the function"""

    @abstractmethod
    def cancel(self):
        """Cancel an in-progress run of the function.
        Must be called from different thread than run().
        Returns true if run was successfully cancelled, false otherwise"""

    @abstractmethod
    def on_complete(self, cancelled=False):
        """Called after run() finishes. If true, cancelled indicates
        cancel() was invoked during the last run()"""

    # Note: The debounce uses threads. Since there is no way to guarantee which
    # thread recieves a KeyboardInterrupt, it is necessary catch this event
    # in all debouncer threads and forward it to the user.
    @abstractmethod
    def on_keyboard_interrupt(self):
        """Called when keyboard interrupt is delivered to a debouncer thread"""


class State(enum.Enum):
    IDLE = 1
    DEBOUNCING = 2
    RUNNING = 3
    INTERRUPTED = 4
    COOLDOWN = 5


class Debouncer:
    """Run an interruptable, cancellable function with debouncing"""
    def __init__(self, function):
        super().__init__()
        self.function = function

        self.state = State.IDLE

        self.debounce_seconds = 1
        self.debounce_timer = None

        self.cooldown_seconds = 1
        self.cooldown_timer = None

        self.lock = threading.Lock()

    def press(self, idle_message=None):
        """Try to run the function for the class. If the function is recently
        started, this may push out the deadline for actually starting. If the
        function is already running, will interrupt the function"""
        _LOG.debug('Press - state = %s', str(self.state))
        with self.lock:
            if self.state == State.IDLE:
                if idle_message:
                    _LOG.info(idle_message)
                self._start_debounce_timer()
                self._transition(State.DEBOUNCING)

            elif self.state == State.DEBOUNCING:
                self._start_debounce_timer()

            elif self.state == State.RUNNING:
                # Function is already running, so do nothing.
                # TODO: It may make sense to queue an automatic re-build
                # when an interruption is detected. Follow up on this after
                # using the interruptable watcher in practice for awhile.

                # Push an empty line to flush ongoing I/O in subprocess.
                print()
                print()
                _LOG.error('File change detected while running')
                _LOG.error('Build may be inconsistent or broken')
                print()
                self.function.cancel()
                self._transition(State.INTERRUPTED)

            elif self.state == State.INTERRUPTED:
                # Function is running but was already interrupted. Do nothing.
                _LOG.debug('Ignoring press - interrupted')

            elif self.state == State.COOLDOWN:
                # Function just finished and we are cooling down, so do nothing.
                _LOG.debug('Ignoring press - cooldown')

    def _transition(self, new_state):
        _LOG.debug('State: %s -> %s', str(self.state), str(new_state))
        self.state = new_state

    def _start_debounce_timer(self):
        assert self.lock.locked()
        if self.state == State.DEBOUNCING:
            self.debounce_timer.cancel()
        self.debounce_timer = threading.Timer(self.debounce_seconds,
                                              self._run_function)
        self.debounce_timer.start()

    # Called from debounce_timer thread.
    def _run_function(self):
        try:
            with self.lock:
                assert self.state == State.DEBOUNCING
                self.debounce_timer = None
                self._transition(State.RUNNING)

            # Must run the function without the lock held so further press()
            # calls don't deadlock.
            _LOG.debug('Running debounced function')
            self.function.run()

            _LOG.debug('Finished running debounced function')
            with self.lock:
                self.function.on_complete(self.state == State.INTERRUPTED)
                self._transition(State.COOLDOWN)
                self._start_cooldown_timer()
        except KeyboardInterrupt:
            self.function.on_keyboard_interrupt()

    def _start_cooldown_timer(self):
        assert self.lock.locked()
        self.cooldown_timer = threading.Timer(self.cooldown_seconds,
                                              self._exit_cooldown)
        self.cooldown_timer.start()

    # Called from cooldown_timer thread.
    def _exit_cooldown(self):
        try:
            with self.lock:
                self.cooldown_timer = None
                self._transition(State.IDLE)
        except KeyboardInterrupt:
            self.function.on_keyboard_interrupt()
