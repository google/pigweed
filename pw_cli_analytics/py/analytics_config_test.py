# Copyright 2025 The Pigweed Authors
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
"""Tests for pw_cli_analytics.config."""


import unittest

from pw_cli_analytics import config


class ConfigLoading(unittest.TestCase):
    """Tests loading analytics configs."""

    def test_config_path(self):
        self.assertIsNotNone(config.DEFAULT_PROJECT_FILE)
        self.assertIsNotNone(config.DEFAULT_PROJECT_USER_FILE)


if __name__ == '__main__':
    unittest.main()
