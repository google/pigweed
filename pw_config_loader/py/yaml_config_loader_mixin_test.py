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
"""Tests for pw_config_loader."""

from pathlib import Path
import tempfile
from typing import Any, Dict
import unittest

from pw_config_loader import yaml_config_loader_mixin
import yaml

# pylint: disable=no-member,no-self-use


class YamlConfigLoader(yaml_config_loader_mixin.YamlConfigLoaderMixin):
    @property
    def config(self) -> Dict[str, Any]:
        return self._config


class TestOneFile(unittest.TestCase):
    """Tests for loading a config section from one file."""

    def setUp(self):
        self._title = 'title'

    def init(self, config: Dict[str, Any]) -> Dict[str, Any]:
        loader = YamlConfigLoader()
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder, 'foo.yaml')
            path.write_bytes(yaml.safe_dump(config).encode())
            loader.config_init(
                user_file=path,
                config_section_title=self._title,
            )
            return loader.config

    def test_normal(self):
        content = {'a': 1, 'b': 2}
        config = self.init({self._title: content})
        self.assertEqual(content['a'], config['a'])
        self.assertEqual(content['b'], config['b'])

    def test_config_title(self):
        content = {'a': 1, 'b': 2, 'config_title': self._title}
        config = self.init(content)
        self.assertEqual(content['a'], config['a'])
        self.assertEqual(content['b'], config['b'])


class TestMultipleFiles(unittest.TestCase):
    """Tests for loading config sections from multiple files."""

    def init(
        self,
        project_config: Dict[str, Any],
        project_user_config: Dict[str, Any],
        user_config: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Write config files then read and parse them."""

        loader = YamlConfigLoader()
        title = 'title'

        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)

            user_path = path / 'user.yaml'
            user_path.write_text(yaml.safe_dump({title: user_config}))

            project_user_path = path / 'project_user.yaml'
            project_user_path.write_text(
                yaml.safe_dump({title: project_user_config})
            )

            project_path = path / 'project.yaml'
            project_path.write_text(yaml.safe_dump({title: project_config}))

            loader.config_init(
                user_file=user_path,
                project_user_file=project_user_path,
                project_file=project_path,
                config_section_title=title,
            )

        return loader.config

    def test_user_override(self):
        config = self.init(
            user_config={'a': 1},
            project_user_config={'a': 2},
            project_config={'a': 3},
        )
        self.assertEqual(config['a'], 1)

    def test_project_user_override(self):
        config = self.init(
            user_config={},
            project_user_config={'a': 2},
            project_config={'a': 3},
        )
        self.assertEqual(config['a'], 2)

    def test_not_overridden(self):
        config = self.init(
            user_config={},
            project_user_config={},
            project_config={'a': 3},
        )
        self.assertEqual(config['a'], 3)

    def test_different_keys(self):
        config = self.init(
            user_config={'a': 1},
            project_user_config={'b': 2},
            project_config={'c': 3},
        )
        self.assertEqual(config['a'], 1)
        self.assertEqual(config['b'], 2)
        self.assertEqual(config['c'], 3)


class TestNestedTitle(unittest.TestCase):
    """Tests for nested config section loading."""

    def setUp(self):
        self._title = ('title', 'subtitle', 'subsubtitle', 'subsubsubtitle')

    def init(self, config: Dict[str, Any]) -> Dict[str, Any]:
        loader = YamlConfigLoader()
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder, 'foo.yaml')
            path.write_bytes(yaml.safe_dump(config).encode())
            loader.config_init(
                user_file=path,
                config_section_title=self._title,
            )
            return loader.config

    def test_normal(self):
        content = {'a': 1, 'b': 2}
        for part in reversed(self._title):
            content = {part: content}
        config = self.init(content)
        self.assertEqual(config['a'], 1)
        self.assertEqual(config['b'], 2)

    def test_config_title(self):
        content = {'a': 1, 'b': 2, 'config_title': '.'.join(self._title)}
        config = self.init(content)
        self.assertEqual(config['a'], 1)
        self.assertEqual(config['b'], 2)


class CustomOverloadYamlConfigLoader(
    yaml_config_loader_mixin.YamlConfigLoaderMixin
):
    """Custom config loader that implements handle_overloaded_value()."""

    @property
    def config(self) -> Dict[str, Any]:
        return self._config

    def handle_overloaded_value(  # pylint: disable=no-self-use
        self,
        key: str,
        stage: yaml_config_loader_mixin.Stage,
        original_value: Any,
        overriding_value: Any,
    ):
        if key == 'extend':
            if original_value:
                return original_value + overriding_value
            return overriding_value

        if key == 'extend_sort':
            if original_value:
                result = original_value + overriding_value
            else:
                result = overriding_value
            return sorted(result)

        if key == 'do_not_override':
            if original_value:
                return original_value

        if key == 'max':
            return max(original_value, overriding_value)

        return overriding_value


class TestOverloading(unittest.TestCase):
    """Tests for envparse.EnvironmentParser."""

    def init(
        self,
        project_config: Dict[str, Any],
        project_user_config: Dict[str, Any],
        user_config: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Write config files then read and parse them."""

        loader = CustomOverloadYamlConfigLoader()
        title = 'title'

        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)

            user_path = path / 'user.yaml'
            user_path.write_text(yaml.safe_dump({title: user_config}))

            project_user_path = path / 'project_user.yaml'
            project_user_path.write_text(
                yaml.safe_dump({title: project_user_config})
            )

            project_path = path / 'project.yaml'
            project_path.write_text(yaml.safe_dump({title: project_config}))

            loader.config_init(
                user_file=user_path,
                project_user_file=project_user_path,
                project_file=project_path,
                config_section_title=title,
            )

        return loader.config

    def test_lists(self):
        config = self.init(
            project_config={
                'extend': list('abc'),
                'extend_sort': list('az'),
                'do_not_override': ['persists'],
                'override': ['hidden'],
            },
            project_user_config={
                'extend': list('def'),
                'extend_sort': list('by'),
                'do_not_override': ['ignored'],
                'override': ['ignored'],
            },
            user_config={
                'extend': list('ghi'),
                'extend_sort': list('cx'),
                'do_not_override': ['ignored_2'],
                'override': ['overrides'],
            },
        )
        self.assertEqual(config['extend'], list('abcdefghi'))
        self.assertEqual(config['extend_sort'], list('abcxyz'))
        self.assertEqual(config['do_not_override'], ['persists'])
        self.assertEqual(config['override'], ['overrides'])

    def test_scalars(self):
        config = self.init(
            project_config={'extend': 'abc', 'max': 1},
            project_user_config={'extend': 'def', 'max': 3},
            user_config={'extend': 'ghi', 'max': 2},
        )
        self.assertEqual(config['extend'], 'abcdefghi')
        self.assertEqual(config['max'], 3)


if __name__ == '__main__':
    unittest.main()
