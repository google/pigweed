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
from typing import Any, Dict, Union

import yaml

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
        'column_order': [],
        'column_colors': {},
        # Window arrangement
        'windows': {},
    },
}


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

    def _update_config(self, cfg: Dict[Any, Any]) -> None:
        assert 'pw_console' in cfg
        self._config.update(cfg.get('pw_console', {}))

    def load_config(self, file_path: Path) -> None:
        if not file_path.is_file():
            return
        cfg = yaml.load(file_path.read_text(), Loader=yaml.Loader)
        self._update_config(cfg)

    @property
    def ui_theme(self) -> str:
        return self._config.get('ui_theme', '')

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
