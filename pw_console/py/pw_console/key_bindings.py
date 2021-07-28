# Copyright 2021 The Pigweed Authors
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
# pylint: skip-file
"""Console key bindings."""
import logging

from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.key_binding.bindings.focus import focus_next, focus_previous

import pw_console.pw_ptpython_repl

__all__ = ('create_key_bindings', )

_LOG = logging.getLogger(__package__)


def create_key_bindings(console_app):
    """Create custom key bindings.

    This starts with the key bindings, defined by `prompt-toolkit`, but adds the
    ones which are specific for the console_app. A console_app instance
    reference is passed in so key bind functions can access it.
    """

    bindings = KeyBindings()

    @bindings.add(
        'f1', filter=Condition(lambda: not console_app.modal_window_is_open()))
    def show_help(event):
        """Toggle user guide window."""
        console_app.user_guide_window.toggle_display()

    # F2 is ptpython settings
    # F3 is ptpython history

    @bindings.add('f4')
    def toggle_vertical_split(event):
        """Toggle horizontal and vertical window splitting."""
        console_app.window_manager.toggle_vertical_split()

    # NOTE: c-up and c-down seem swapped in prompt_toolkit
    @bindings.add('escape', 'c-up')  # Alt-Ctrl-up
    @bindings.add('escape', 'c-j')  # Alt-Ctrl-j
    def move_pane_down(event):
        """Move window pane down."""
        console_app.window_manager.move_pane_down()

    # NOTE: c-up and c-down seem swapped in prompt_toolkit
    @bindings.add('escape', 'c-down')  # Alt-Ctrl-down
    @bindings.add('escape', 'c-k')  # Alt-Ctrl-k
    def move_pane_up(event):
        """Move window pane up."""
        console_app.window_manager.move_pane_up()

    @bindings.add('c-j')
    def enlarge_pane(event):
        """Enlarge the active window pane."""
        console_app.window_manager.enlarge_pane()

    @bindings.add('c-k')
    def shrink_pane(event):
        """Shrink the active window pane."""
        console_app.window_manager.shrink_pane()

    @bindings.add('c-u')
    def balance_window_panes(event):
        """Balance all window pane sizes."""
        console_app.window_manager.reset_pane_sizes()

    @bindings.add('c-w')
    @bindings.add('c-q')
    def exit_(event):
        """Quit the console application."""
        # TODO(tonymd): Cancel any existing user repl or plugin tasks before
        # exiting.
        event.app.exit()

    @bindings.add('s-tab')
    @bindings.add('c-right')
    @bindings.add('c-down')
    def app_focus_next(event):
        """Move focus to the next widget."""
        focus_next(event)

    @bindings.add('c-left')
    @bindings.add('c-up')
    def app_focus_previous(event):
        """Move focus to the previous widget."""
        focus_previous(event)

    # Bindings for when the ReplPane input field is in focus.
    # These are hidden from help window global keyboard shortcuts since the
    # method names end with `_hidden`.
    @bindings.add('c-c', filter=has_focus(console_app.pw_ptpython_repl))
    def handle_ctrl_c_hidden(event):
        """Reset the python repl on Ctrl-c"""
        console_app.repl_pane.ctrl_c()

    @bindings.add('c-d',
                  filter=console_app.pw_ptpython_repl.
                  has_focus_and_input_empty_condition())
    def handle_ctrl_d_hidden(event):
        """Do nothing on ctrl-d."""
        # TODO(tonymd): Ctrl-d should quit the whole app with confirmation.
        # Ctrl-d in ptpython repl prompts y/n to quit if the input is empty. If
        # not empty, it deletes forward characters. This binding should block
        # ctrl-d if the input is empty. The quit confirmation dialog breaks the
        # ptpython state and further repl invocations.
        pass

    return bindings
