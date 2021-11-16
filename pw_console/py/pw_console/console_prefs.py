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
"""pw_console preferences"""

from dataclasses import dataclass, field
import os
from pathlib import Path
from typing import Any, Dict, List, Union

import yaml

from pw_console.style import get_theme_colors

_DEFAULT_REPL_HISTORY: Path = Path.home() / '.pw_console_history'
_DEFAULT_SEARCH_HISTORY: Path = Path.home() / '.pw_console_search'

_DEFAULT_CONFIG = {
    'pw_console': {
        # History files
        'repl_history': _DEFAULT_REPL_HISTORY,
        'search_history': _DEFAULT_SEARCH_HISTORY,
        # Appearance
        'ui_theme': 'dark',
        'code_theme': 'pigweed-code',
        'swap_light_and_dark': False,
        'spaces_between_columns': 2,
        'column_order_omit_unspecified_columns': False,
        'column_order': [],
        'column_colors': {},
        'show_python_file': False,
        'show_python_logger': False,
        'hide_date_from_log_time': False,
        # Window arrangement
        'windows': {},
        'window_column_split_method': 'vertical',
    },
}


class UnknownWindowTitle(Exception):
    """Exception for window titles not present in the window manager layout."""


def error_unknown_window(window_title: str,
                         existing_pane_titles: List[str]) -> None:
    """Raise an error when the window config has an unknown title.

    If a window title does not already exist on startup it must have a loggers:
    or duplicate_of: option set."""

    pane_title_text = '  ' + '\n  '.join(existing_pane_titles)
    existing_pane_title_example = 'Window Title'
    if existing_pane_titles:
        existing_pane_title_example = existing_pane_titles[0]
    raise UnknownWindowTitle(
        f'\n\n"{window_title}" does not exist.\n'
        'Existing windows include:\n'
        f'{pane_title_text}\n'
        'If this window should be a duplicate of one of the above,\n'
        f'add "duplicate_of: {existing_pane_title_example}" to your config.\n'
        'If this is a brand new window, include a "loggers:" section.\n'
        'See also: '
        'https://pigweed.dev/pw_console/docs/user_guide.html#example-config')


@dataclass
class ConsolePrefs:
    """Pigweed Console preferences storage class."""

    project_file: Union[Path, bool] = Path('$PW_PROJECT_ROOT/.pw_console.yaml')
    user_file: Union[Path, bool] = Path('$HOME/.pw_console.yaml')
    _config: Dict[Any, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        self._update_config(_DEFAULT_CONFIG)

        if self.project_file:
            assert isinstance(self.project_file, Path)
            self.project_file = Path(
                os.path.expandvars(str(self.project_file.expanduser())))
            self.load_config(self.project_file)

        if self.user_file:
            assert isinstance(self.user_file, Path)
            self.user_file = Path(
                os.path.expandvars(str(self.user_file.expanduser())))
            self.load_config(self.user_file)

        # Check for a config file specified by an environment variable.
        environment_config = os.environ.get('PW_CONSOLE_CONFIG_FILE', None)
        if environment_config:
            env_file_path = Path(environment_config)
            if not env_file_path.is_file():
                raise FileNotFoundError(
                    f'Cannot load config file: {env_file_path}')
            self.reset_config()
            self.load_config(env_file_path)

    def _update_config(self, cfg: Dict[Any, Any]) -> None:
        assert 'pw_console' in cfg
        self._config.update(cfg.get('pw_console', {}))

    def reset_config(self) -> None:
        self._config = {}
        self._update_config(_DEFAULT_CONFIG)

    def load_config(self, file_path: Path) -> None:
        if not file_path.is_file():
            return
        cfg = yaml.load(file_path.read_text(), Loader=yaml.Loader)
        self._update_config(cfg)

    @property
    def ui_theme(self) -> str:
        return self._config.get('ui_theme', '')

    def set_ui_theme(self, theme_name: str):
        self._config['ui_theme'] = theme_name

    @property
    def theme_colors(self):
        return get_theme_colors(self.ui_theme)

    @property
    def code_theme(self) -> str:
        return self._config.get('code_theme', '')

    @property
    def swap_light_and_dark(self) -> bool:
        return self._config.get('swap_light_and_dark', False)

    @property
    def repl_history(self) -> Path:
        history = Path(self._config['repl_history'])
        history = Path(os.path.expandvars(str(history.expanduser())))
        return history

    @property
    def search_history(self) -> Path:
        history = Path(self._config['search_history'])
        history = Path(os.path.expandvars(str(history.expanduser())))
        return history

    @property
    def spaces_between_columns(self) -> int:
        spaces = self._config.get('spaces_between_columns', 2)
        assert isinstance(spaces, int) and spaces > 0
        return spaces

    @property
    def omit_unspecified_columns(self) -> bool:
        return self._config.get('column_order_omit_unspecified_columns', False)

    @property
    def hide_date_from_log_time(self) -> bool:
        return self._config.get('hide_date_from_log_time', False)

    @property
    def show_python_file(self) -> bool:
        return self._config.get('show_python_file', False)

    @property
    def show_python_logger(self) -> bool:
        return self._config.get('show_python_logger', False)

    def toggle_bool_option(self, name: str):
        existing_setting = self._config[name]
        assert isinstance(existing_setting, bool)
        self._config[name] = not existing_setting

    @property
    def column_order(self) -> list:
        return self._config.get('column_order', [])

    def column_style(self,
                     column_name: str,
                     column_value: str,
                     default='') -> str:
        column_colors = self._config.get('column_colors', {})
        column_style = default

        if column_name in column_colors:
            # If key exists but doesn't have any values.
            if not column_colors[column_name]:
                return default
            # Check for user supplied default.
            column_style = column_colors[column_name].get('default', default)
            # Check for value specific color, otherwise use the default.
            column_style = column_colors[column_name].get(
                column_value, column_style)
        return column_style

    @property
    def window_column_split_method(self) -> str:
        return self._config.get('window_column_split_method', 'vertical')

    @property
    def windows(self) -> dict:
        return self._config.get('windows', {})

    @property
    def window_column_modes(self) -> list:
        return list(column_type for column_type in self.windows.keys())

    @property
    def unique_window_titles(self) -> set:
        titles = []
        for column in self.windows.values():
            for window_key_title, window_dict in column.items():
                window_options = window_dict if window_dict else {}
                # Use 'duplicate_of: Title' if it exists, otherwise use the key.
                titles.append(
                    window_options.get('duplicate_of', window_key_title))
        return set(titles)
