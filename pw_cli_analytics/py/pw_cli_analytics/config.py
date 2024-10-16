# Copyright 2024 The Pigweed Authors
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
"""Report usage to Google Analytics."""

import logging
import os
from pathlib import Path
from typing import Any, Generator

import requests  # pylint: disable=unused-import

import pw_config_loader.json_config_loader_mixin

_LOG: logging.Logger = logging.getLogger(__name__)

_PW_PROJECT_ROOT = Path(os.environ['PW_PROJECT_ROOT'])

CONFIG_SECTION_TITLE = ('pw', 'pw_cli_analytics')
DEFAULT_PROJECT_FILE = _PW_PROJECT_ROOT / 'pigweed.json'
DEFAULT_PROJECT_USER_FILE = _PW_PROJECT_ROOT / '.pw_cli_analytics.user.json'
DEFAULT_USER_FILE = Path(os.path.expanduser('~/.pw_cli_analytics.json'))
ENVIRONMENT_VAR = 'PW_CLI_ANALYTICS_CONFIG_FILE'

_DEFAULT_CONFIG = {
    'api_secret': 'm7q0D-9ETtKrGqHAcQK2kQ',
    'measurement_id': 'G-NY45VS0X1F',
    'debug_url': 'https://www.google-analytics.com/debug/mp/collect',
    'prod_url': 'https://www.google-analytics.com/mp/collect',
    'report_command_line': False,
    'report_project_name': False,
    'report_remote_url': False,
    'report_subcommand_name': 'limited',
    'uuid': None,
    'enabled': None,
}


class AnalyticsPrefs(
    pw_config_loader.json_config_loader_mixin.JsonConfigLoaderMixin
):
    """Preferences for reporting analytics data."""

    def __init__(
        self,
        *,
        project_file: Path | None = DEFAULT_PROJECT_FILE,
        project_user_file: Path | None = DEFAULT_PROJECT_USER_FILE,
        user_file: Path | None = DEFAULT_USER_FILE,
        **kwargs,
    ) -> None:
        super().__init__(**kwargs)

        self.config_init(
            config_section_title=CONFIG_SECTION_TITLE,
            project_file=project_file,
            project_user_file=project_user_file,
            user_file=user_file,
            default_config=_DEFAULT_CONFIG,
            environment_var=ENVIRONMENT_VAR,
            skip_files_without_sections=True,
        )

    def __iter__(self):
        return iter(_DEFAULT_CONFIG.keys())

    def __getitem__(self, key):
        return self._config[key]

    def handle_overloaded_value(  # pylint: disable=no-self-use
        self,
        key: str,
        stage: pw_config_loader.json_config_loader_mixin.Stage,
        original_value: Any,
        overriding_value: Any,
    ) -> Any:
        """Overload this in subclasses to handle of overloaded values."""
        Stage = pw_config_loader.json_config_loader_mixin.Stage

        # This is a user-specific value. Don't accept it from anywhere but the
        # user file.
        if key == 'uuid':
            if stage == Stage.USER_FILE:
                return overriding_value
            return original_value

        # If any level says that data collection should be disabled, disable it.
        # The default value is None, not False, and the default in generated
        # user files is True. But if the project says enabled is False we'll
        # keep that, regardless of what any other files say.
        if key == 'enabled':
            if original_value is False:
                return original_value
            return overriding_value

        # URLs can by changed by any config.
        if key in ('debug_url', 'prod_url'):
            return overriding_value

        # What's left is the details of what to report. In general, these should
        # only be set by the project file and the user/project file.
        if stage in (Stage.PROJECT_FILE, Stage.USER_PROJECT_FILE):
            return overriding_value

        # Only honor user file settings about what to report when they disable
        # things. Don't honor them when they enable things.
        if stage == Stage.USER_FILE:
            if overriding_value in (False, 'never'):
                return overriding_value
            if original_value == 'always' and overriding_value == 'limited':
                return overriding_value

        return original_value

    def items(self) -> Generator[tuple[str, Any], None, None]:
        """Yield all the key/value pairs in the config."""
        for key in _DEFAULT_CONFIG.keys():
            yield (key, self._config[key])
