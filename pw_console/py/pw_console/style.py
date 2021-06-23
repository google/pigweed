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

import logging
from dataclasses import dataclass

from prompt_toolkit.styles import Style

_LOG = logging.getLogger(__package__)


# pylint: disable=too-many-instance-attributes
@dataclass
class HighContrastDarkColors:
    default_bg = '#100f10'
    default_fg = '#ffffff'

    dim_bg = '#000000'
    dim_fg = '#e0e6f0'

    active_bg = '#323232'
    active_fg = '#f4f4f4'

    inactive_bg = '#1e1e1e'
    inactive_fg = '#bfc0c4'

    line_highlight_bg = '#2f2f2f'
    dialog_bg = '#3c3c3c'

    blue_accent = '#92d9ff'
    cyan_accent = '#60e7e0'
    green_accent = '#88ef88'
    magenta_accent = '#ffb8ff'
    orange_accent = '#f5ca80'
    purple_accent = '#cfcaff'
    red_accent = '#ffc0bf'
    yellow_accent = '#d2e580'


# pylint: disable=too-many-instance-attributes
@dataclass
class DarkColors:
    default_bg = '#2e2e2e'
    default_fg = '#bbc2cf'

    dim_bg = '#262626'
    dim_fg = '#dfdfdf'

    active_bg = '#525252'
    active_fg = '#dfdfdf'

    inactive_bg = '#3f3f3f'
    inactive_fg = '#bfbfbf'

    line_highlight_bg = '#1e1e1e'
    dialog_bg = '#3c3c3c'

    blue_accent = '#51afef'
    cyan_accent = '#46d9ff'
    green_accent = '#98be65'
    magenta_accent = '#c678dd'
    orange_accent = '#da8548'
    purple_accent = '#a9a1e1'
    red_accent = '#ff6c6b'
    yellow_accent = '#ecbe7b'


_THEME_NAME_MAPPING = {
    'dark': DarkColors(),
    'high-contrast-dark': HighContrastDarkColors(),
} # yapf: disable

def generate_styles(theme_name='dark'):
    """Return prompt_toolkit styles for the given theme name."""
    theme = _THEME_NAME_MAPPING.get(theme_name, DarkColors())

    pw_console_styles = {
        # Default text and background.
        'default': 'bg:{} {}'.format(theme.default_bg, theme.default_fg),
        # Dim inactive panes.
        'pane_inactive': 'bg:{} {}'.format(theme.dim_bg, theme.dim_fg),
        # Use default for active panes.
        'pane_active': 'bg:{} {}'.format(theme.default_bg, theme.default_fg),

        # Brighten active pane toolbars.
        'toolbar_active': 'bg:{} {}'.format(theme.active_bg, theme.active_fg),
        'toolbar_inactive': 'bg:{} {}'.format(theme.inactive_bg,
                                              theme.inactive_fg),
        # Used for pane titles
        'toolbar_accent': theme.cyan_accent,

        # prompt_toolkit scrollbar styles:
        'scrollbar.background': 'bg:{} {}'.format(theme.default_bg,
                                                  theme.default_fg),
        # Scrollbar handle, bg is the bar color.
        'scrollbar.button': 'bg:{} {}'.format(theme.purple_accent,
                                              theme.default_bg),
        'scrollbar.arrow': 'bg:{} {}'.format(theme.default_bg,
                                             theme.blue_accent),
        # Unstyled scrollbar classes:
        # 'scrollbar.start'
        # 'scrollbar.end'

        # Top menu bar styles
        'menu-bar': 'bg:{} {}'.format(theme.inactive_bg, theme.inactive_fg),
        'menu-bar.selected-item': 'bg:{} {}'.format(theme.blue_accent,
                                                    theme.inactive_bg),
        # Menu background
        'menu': 'bg:{} {}'.format(theme.dialog_bg, theme.dim_fg),
        # Menu item separator
        'menu-border': theme.magenta_accent,

        # Top bar logo + keyboard shortcuts
        'logo':    '{} bold'.format(theme.magenta_accent),
        'keybind': '{} bold'.format(theme.blue_accent),
        'keyhelp': theme.dim_fg,

        # Help window styles
        'help_window_content': 'bg:{} {}'.format(theme.dialog_bg, theme.dim_fg),
        'frame.border': 'bg:{} {}'.format(theme.dialog_bg, theme.purple_accent),

        'pane_indicator_active': 'bg:{}'.format(theme.magenta_accent),
        'pane_indicator_inactive': 'bg:{}'.format(theme.inactive_bg),

        'pane_title_active': '{} bold'.format(theme.magenta_accent),
        'pane_title_inactive': '{}'.format(theme.purple_accent),

        'pane_separator': 'bg:{} {}'.format(theme.default_bg,
                                            theme.purple_accent),

        # Highlighted line styles
        'selected-log-line': 'bg:{}'.format(theme.line_highlight_bg),
        'cursor-line': 'bg:{} nounderline'.format(theme.line_highlight_bg),

        # Messages like 'Window too small'
        'warning-text': 'bg:{} {}'.format(theme.default_bg, theme.yellow_accent)
    } # yapf: disable

    return Style.from_dict(pw_console_styles)
