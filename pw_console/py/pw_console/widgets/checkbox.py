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


def to_checkbox(checked: bool, mouse_handler=None, end=' '):
    default_style = 'class:checkbox'
    checked_style = 'class:checkbox-checked'
    text = '[x]' if checked else '[ ]'
    text += end
    style = checked_style if checked else default_style
    if mouse_handler:
        return (style, text, mouse_handler)
    return (style, text)


def to_checkbox_text(checked: bool, end=' '):
    return to_checkbox(checked, end=end)[1]
