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
"""Tests for pw_console.console_app"""

from datetime import datetime
from pathlib import Path
import tempfile
import unittest

import yaml

# pylint: disable=protected-access
from pw_console.console_prefs import (
    ConsolePrefs,
    _DEFAULT_CONFIG,
)


def _create_tempfile(content: str) -> Path:
    # Grab the current system timestamp as a string.
    isotime = datetime.now().isoformat(sep='_', timespec='seconds')
    isotime = isotime.replace(':', '')

    with tempfile.NamedTemporaryFile(prefix=f'{__package__}_{isotime}_',
                                     delete=False) as output_file:
        file_path = Path(output_file.name)
        output_file.write(content.encode('UTF-8'))
    return file_path


class TestConsolePrefs(unittest.TestCase):
    """Tests for ConsolePrefs."""
    def setUp(self):
        self.maxDiff = None  # pylint: disable=invalid-name

    def test_load_no_existing_files(self) -> None:
        prefs = ConsolePrefs(project_file=False, user_file=False)
        self.assertEqual(_DEFAULT_CONFIG['pw_console'], prefs._config)
        self.assertTrue(str(prefs.repl_history).endswith('pw_console_history'))
        self.assertTrue(
            str(prefs.search_history).endswith('pw_console_search'))

    def test_load_project_file(self) -> None:
        project_config = {
            'pw_console': {
                'ui_theme': 'light',
                'code_theme': 'cool-code',
                'swap_light_and_dark': True,
            },
        }
        project_config_file = _create_tempfile(yaml.dump(project_config))
        try:
            prefs = ConsolePrefs(project_file=project_config_file,
                                 user_file=False)
            result_settings = {
                k: v
                for k, v in prefs._config.items()
                if k in project_config['pw_console'].keys()
            }
            other_settings = {
                k: v
                for k, v in prefs._config.items()
                if k not in project_config['pw_console'].keys()
            }
            self.assertEqual(project_config['pw_console'], result_settings)
            self.assertNotEqual(0, len(other_settings))
        finally:
            project_config_file.unlink()

    def test_load_project_and_user_file(self) -> None:
        """Test user settings override project settings."""
        project_config = {
            'pw_console': {
                'ui_theme': 'light',
                'code_theme': 'cool-code',
                'swap_light_and_dark': True,
                'repl_history': '~/project_history',
                'search_history': '~/project_search',
            },
        }
        project_config_file = _create_tempfile(yaml.dump(project_config))

        user_config = {
            'pw_console': {
                'ui_theme': 'dark',
                'search_history': '~/user_search',
            },
        }
        user_config_file = _create_tempfile(yaml.dump(user_config))
        try:
            prefs = ConsolePrefs(project_file=project_config_file,
                                 user_file=user_config_file)
            # Set by the project
            self.assertEqual(project_config['pw_console']['code_theme'],
                             prefs.code_theme)
            self.assertEqual(
                project_config['pw_console']['swap_light_and_dark'],
                prefs.swap_light_and_dark)

            history = project_config['pw_console']['repl_history']
            assert isinstance(history, str)
            self.assertEqual(Path(history).expanduser(), prefs.repl_history)

            # User config overrides project
            self.assertEqual(user_config['pw_console']['ui_theme'],
                             prefs.ui_theme)
            self.assertEqual(
                Path(user_config['pw_console']['search_history']).expanduser(),
                prefs.search_history)
        finally:
            project_config_file.unlink()
            user_config_file.unlink()


if __name__ == '__main__':
    unittest.main()
