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
"""Functions to create checkboxes for menus and toolbars."""

from typing import Iterable, Optional

from prompt_toolkit.formatted_text import StyleAndTextTuples

_KEY_SEPARATOR = ' → '


def to_checkbox(checked: bool, mouse_handler=None, end=' '):
    default_style = 'class:checkbox'
    checked_style = 'class:checkbox-checked'
    text = '[✓]' if checked else '[ ]'
    text += end
    style = checked_style if checked else default_style
    if mouse_handler:
        return (style, text, mouse_handler)
    return (style, text)


def to_checkbox_text(checked: bool, end=' '):
    return to_checkbox(checked, end=end)[1]


def to_setting(
    checked: bool,
    text: str,
    active_style='class:toolbar-setting-active',
    inactive_style='',
    mouse_handler=None,
):
    """Apply a style to text if checked is True."""
    style = active_style if checked else inactive_style
    if mouse_handler:
        return (style, text, mouse_handler)
    return (style, text)


def to_checkbox_with_keybind_indicator(checked: bool,
                                       key: str,
                                       description: str,
                                       mouse_handler=None):
    """Create a clickable keybind indicator with checkbox for toolbars."""
    if mouse_handler:
        return to_keybind_indicator(
            key,
            description,
            mouse_handler,
            extra_fragments=[to_checkbox(checked, mouse_handler)])
    return to_keybind_indicator(key,
                                description,
                                extra_fragments=[to_checkbox(checked)])


def to_keybind_indicator(key: str,
                         description: str,
                         mouse_handler=None,
                         extra_fragments: Optional[Iterable] = None):
    """Create a clickable keybind indicator for toolbars."""
    fragments: StyleAndTextTuples = []

    if mouse_handler:
        fragments.append(('class:keybind', key, mouse_handler))
        fragments.append(('class:keyhelp', _KEY_SEPARATOR, mouse_handler))
    else:
        fragments.append(('class:keybind', key))
        fragments.append(('class:keyhelp', _KEY_SEPARATOR))

    if extra_fragments:
        for fragment in extra_fragments:
            fragments.append(fragment)

    if mouse_handler:
        fragments.append(('class:keyhelp', description, mouse_handler))
    else:
        fragments.append(('class:keyhelp', description))
    return fragments
