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
"""Tests for the config_file module."""

import unittest

from pw_env_setup import config_file


class TestConfigFile(unittest.TestCase):
    def test_config_load(self):
        config = config_file.load()
        # We should have loaded upstream Pigweed's pigweed.json, which will
        # contain a top-level "pw" section.
        self.assertIn(config, "pw")
