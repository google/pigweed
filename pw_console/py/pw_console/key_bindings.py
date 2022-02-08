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

    @bindings.add('c-left')
    def app_focus_previous(event):
        """Move focus to the previous widget."""
        focus_previous(event)

    @bindings.add('s-tab')
    @bindings.add('c-right')
    def app_focus_next(event):
        """Move focus to the next widget."""
        focus_next(event)

    # Bindings for when the ReplPane input field is in focus.
    # These are hidden from help window global keyboard shortcuts since the
    # method names end with `_hidden`.
    @bindings.add('c-c', filter=has_focus(console_app.pw_ptpython_repl))
    def handle_ctrl_c_hidden(event):
        """Reset the python repl on Ctrl-c"""
        console_app.repl_pane.ctrl_c()

    @bindings.add('c-x', 'c-c')
    def quit_no_confirm(event):
        """Quit without confirmation."""
        event.app.exit()

    @bindings.add(
        'c-d',
        filter=console_app.pw_ptpython_repl.input_empty_if_in_focus_condition(
        ) | has_focus(console_app.quit_dialog))
    def quit(event):
        """Quit with confirmation dialog."""
        # If the python repl is in focus and has text input then Ctrl-d will
        # delete forward characters instead.
        console_app.quit_dialog.open_dialog()

    @bindings.add('c-v', filter=has_focus(console_app.pw_ptpython_repl))
    def paste_into_repl(event):
        """Reset the python repl on Ctrl-c"""
        console_app.repl_pane.paste_system_clipboard_to_input_buffer()

    @bindings.add(
        'escape',
        'c-c',  # Alt-Ctrl-c
        filter=console_app.repl_pane.input_or_output_has_focus())
    def paste_into_repl(event):
        """Copy all Python output to the system clipboard."""
        console_app.repl_pane.copy_text()

    return bindings
