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
"""UI Color Styles for ConsoleApp."""

from prompt_toolkit.styles import Style

TOOLBAR_STYLE = 'bg:#fdd1ff #432445'

pw_console_styles = Style.from_dict({
    'bottom_toolbar_colored_background': TOOLBAR_STYLE,
    'bottom_toolbar': TOOLBAR_STYLE,
    'bottom_toolbar_colored_text': TOOLBAR_STYLE,

    # FloatingMessageBar style
    'message': 'bg:#282c34 #c678dd',

    # prompt_toolkit scrollbar styles:
    'scrollbar.background': 'bg:#3e4452 #abb2bf',
    'scrollbar.button': 'bg:#7f3285 #282c34',
    # Unstyled scrollbar classes:
    # 'scrollbar.arrow'
    # 'scrollbar.start'
    # 'scrollbar.end'

    # Top menu bar styles
    'menu': 'bg:#3e4452 #bbc2cf',
    'menu-bar.selected-item': 'bg:#61afef #282c34',
    'menu-border': 'bg:#282c34 #61afef',
    'menu-bar': TOOLBAR_STYLE,

    # Top bar logo + keyboard shortcuts
    'logo':    TOOLBAR_STYLE + ' bold',
    'keybind': TOOLBAR_STYLE,
    'keyhelp': TOOLBAR_STYLE,

    # Help window styles
    'help_window_content': 'bg:default default',
    'frame.border': '',
    'shadow': 'bg:#282c34',

    # Highlighted line style
    'cursor-line': 'bg:#3e4452 nounderline',
    'selected-log-line': 'bg:#3e4452',
}) # yapf: disable
