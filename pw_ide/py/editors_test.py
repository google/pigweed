# Copyright 2022 The Pigweed Authors
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
"""Tests for pw_ide.editors"""

from collections import OrderedDict
from enum import Enum
import unittest

from pw_ide.editors import (
    dict_deep_merge,
    dict_swap_type,
    EditorSettingsFile,
    EditorSettingsManager,
    JsonFileFormat,
    Json5FileFormat,
    YamlFileFormat,
    _StructuredFileFormat,
)

from test_cases import PwIdeTestCase


class TestDictDeepMerge(unittest.TestCase):
    """Tests dict_deep_merge"""

    # pylint: disable=unnecessary-lambda
    def test_invariants_with_dict_success(self):
        dict_a = {'hello': 'world'}
        dict_b = {'foo': 'bar'}

        expected = {
            'hello': 'world',
            'foo': 'bar',
        }

        result = dict_deep_merge(dict_b, dict_a, lambda: dict())
        self.assertEqual(result, expected)

    def test_invariants_with_dict_implicit_ctor_success(self):
        dict_a = {'hello': 'world'}
        dict_b = {'foo': 'bar'}

        expected = {
            'hello': 'world',
            'foo': 'bar',
        }

        result = dict_deep_merge(dict_b, dict_a)
        self.assertEqual(result, expected)

    def test_invariants_with_dict_fails_wrong_ctor_type(self):
        dict_a = {'hello': 'world'}
        dict_b = {'foo': 'bar'}

        with self.assertRaises(TypeError):
            dict_deep_merge(dict_b, dict_a, lambda: OrderedDict())

    def test_invariants_with_ordered_dict_success(self):
        dict_a = OrderedDict({'hello': 'world'})
        dict_b = OrderedDict({'foo': 'bar'})

        expected = OrderedDict(
            {
                'hello': 'world',
                'foo': 'bar',
            }
        )

        result = dict_deep_merge(dict_b, dict_a, lambda: OrderedDict())
        self.assertEqual(result, expected)

    def test_invariants_with_ordered_dict_implicit_ctor_success(self):
        dict_a = OrderedDict({'hello': 'world'})
        dict_b = OrderedDict({'foo': 'bar'})

        expected = OrderedDict(
            {
                'hello': 'world',
                'foo': 'bar',
            }
        )

        result = dict_deep_merge(dict_b, dict_a)
        self.assertEqual(result, expected)

    def test_invariants_with_ordered_dict_fails_wrong_ctor_type(self):
        dict_a = OrderedDict({'hello': 'world'})
        dict_b = OrderedDict({'foo': 'bar'})

        with self.assertRaises(TypeError):
            dict_deep_merge(dict_b, dict_a, lambda: dict())

    # pylint: enable=unnecessary-lambda


class TestDictSwapType(unittest.TestCase):
    """Tests dict_swap_type"""

    def test_ordereddict_to_dict(self):
        """Test converting an OrderedDict to a plain dict"""

        ordered_dict = OrderedDict(
            {
                'hello': 'world',
                'foo': 'bar',
                'nested': OrderedDict(
                    {
                        'lorem': 'ipsum',
                        'dolor': 'sit amet',
                    }
                ),
            }
        )

        plain_dict = dict_swap_type(ordered_dict, dict)

        expected_plain_dict = {
            'hello': 'world',
            'foo': 'bar',
            'nested': {
                'lorem': 'ipsum',
                'dolor': 'sit amet',
            },
        }

        # The returned dict has the content and type we expect
        self.assertDictEqual(plain_dict, expected_plain_dict)
        self.assertIsInstance(plain_dict, dict)
        self.assertIsInstance(plain_dict['nested'], dict)

        # The original OrderedDict is unchanged
        self.assertIsInstance(ordered_dict, OrderedDict)
        self.assertIsInstance(ordered_dict['nested'], OrderedDict)


class EditorSettingsTestType(Enum):
    SETTINGS = 'settings'


class TestCasesGenericOnFileFormat:
    """Container for tests generic on FileFormat.

    This misdirection is needed to prevent the base test class cases from being
    run as actual tests.
    """

    class EditorSettingsFileTestCase(PwIdeTestCase):
        """Test case for EditorSettingsFile with a provided FileFormat"""

        def setUp(self):
            if not hasattr(self, 'file_format'):
                self.file_format = _StructuredFileFormat()
            return super().setUp()

        def test_open_new_file_and_write(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            with open(
                self.temp_dir_path / f'{name}.{self.file_format.ext}'
            ) as file:
                settings_dict = self.file_format.load(file)

            self.assertEqual(settings_dict['hello'], 'world')

        def test_open_new_file_and_get(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            settings_dict = settings_file.get()
            self.assertEqual(settings_dict['hello'], 'world')

        def test_open_new_file_no_backup(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            backup_files = [
                path
                for path in self.temp_dir_path.iterdir()
                if path.name != f'{name}.{self.file_format.ext}'
            ]

            self.assertEqual(len(backup_files), 0)

        def test_open_existing_file_and_backup(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            with settings_file.modify() as settings:
                settings['hello'] = 'mundo'

            settings_dict = settings_file.get()
            self.assertEqual(settings_dict['hello'], 'mundo')

            backup_files = [
                path
                for path in self.temp_dir_path.iterdir()
                if path.name != f'{name}.{self.file_format.ext}'
            ]

            self.assertEqual(len(backup_files), 1)

            with open(backup_files[0]) as file:
                settings_dict = self.file_format.load(file)

            self.assertEqual(settings_dict['hello'], 'world')

        def test_open_existing_file_with_reinit_and_backup(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            with settings_file.modify(reinit=True) as settings:
                settings['hello'] = 'mundo'

            settings_dict = settings_file.get()
            self.assertEqual(settings_dict['hello'], 'mundo')

            backup_files = [
                path
                for path in self.temp_dir_path.iterdir()
                if path.name != f'{name}.{self.file_format.ext}'
            ]

            self.assertEqual(len(backup_files), 1)

            with open(backup_files[0]) as file:
                settings_dict = self.file_format.load(file)

            self.assertEqual(settings_dict['hello'], 'world')

        def test_open_existing_file_no_change_no_backup(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            settings_dict = settings_file.get()
            self.assertEqual(settings_dict['hello'], 'world')

            backup_files = [
                path
                for path in self.temp_dir_path.iterdir()
                if path.name != f'{name}.{self.file_format.ext}'
            ]

            self.assertEqual(len(backup_files), 0)

        def test_write_bad_file_restore_backup(self):
            name = 'settings'
            settings_file = EditorSettingsFile(
                self.temp_dir_path, name, self.file_format
            )

            with settings_file.modify() as settings:
                settings['hello'] = 'world'

            with self.assertRaises(self.file_format.unserializable_error):
                with settings_file.modify() as settings:
                    settings['hello'] = object()

            settings_dict = settings_file.get()
            self.assertEqual(settings_dict['hello'], 'world')

            backup_files = [
                path
                for path in self.temp_dir_path.iterdir()
                if path.name != f'{name}.{self.file_format.ext}'
            ]

            self.assertEqual(len(backup_files), 0)

    class EditorSettingsManagerTestCase(PwIdeTestCase):
        """Test case for EditorSettingsManager with a provided FileFormat"""

        def setUp(self):
            if not hasattr(self, 'file_format'):
                self.file_format = _StructuredFileFormat()
            return super().setUp()

        def test_settings_merge(self):
            """Test that settings merge as expected in isolation."""
            default_settings = OrderedDict(
                {
                    'foo': 'bar',
                    'baz': 'qux',
                    'lorem': OrderedDict(
                        {
                            'ipsum': 'dolor',
                        }
                    ),
                }
            )

            types_with_defaults = {
                EditorSettingsTestType.SETTINGS: lambda _: default_settings
            }

            ide_settings = self.make_ide_settings()
            manager = EditorSettingsManager(
                ide_settings,
                self.temp_dir_path,
                self.file_format,
                types_with_defaults,
            )

            project_settings = OrderedDict(
                {
                    'alpha': 'beta',
                    'baz': 'xuq',
                    'foo': 'rab',
                }
            )

            with manager.project(
                EditorSettingsTestType.SETTINGS
            ).modify() as settings:
                dict_deep_merge(project_settings, settings)

            user_settings = OrderedDict(
                {
                    'baz': 'xqu',
                    'lorem': OrderedDict(
                        {
                            'ipsum': 'sit amet',
                            'consectetur': 'adipiscing',
                        }
                    ),
                }
            )

            with manager.user(
                EditorSettingsTestType.SETTINGS
            ).modify() as settings:
                dict_deep_merge(user_settings, settings)

            expected = {
                'alpha': 'beta',
                'foo': 'rab',
                'baz': 'xqu',
                'lorem': {
                    'ipsum': 'sit amet',
                    'consectetur': 'adipiscing',
                },
            }

            with manager.active(
                EditorSettingsTestType.SETTINGS
            ).modify() as active_settings:
                manager.default(EditorSettingsTestType.SETTINGS).sync_to(
                    active_settings
                )
                manager.project(EditorSettingsTestType.SETTINGS).sync_to(
                    active_settings
                )
                manager.user(EditorSettingsTestType.SETTINGS).sync_to(
                    active_settings
                )

            self.assertCountEqual(
                manager.active(EditorSettingsTestType.SETTINGS).get(), expected
            )


class TestEditorSettingsFileJsonFormat(
    TestCasesGenericOnFileFormat.EditorSettingsFileTestCase
):
    """Test EditorSettingsFile with JsonFormat"""

    def setUp(self):
        self.file_format = JsonFileFormat()
        return super().setUp()


class TestEditorSettingsManagerJsonFormat(
    TestCasesGenericOnFileFormat.EditorSettingsManagerTestCase
):
    """Test EditorSettingsManager with JsonFormat"""

    def setUp(self):
        self.file_format = JsonFileFormat()
        return super().setUp()


class TestEditorSettingsFileJson5Format(
    TestCasesGenericOnFileFormat.EditorSettingsFileTestCase
):
    """Test EditorSettingsFile with Json5Format"""

    def setUp(self):
        self.file_format = Json5FileFormat()
        return super().setUp()


class TestEditorSettingsManagerJson5Format(
    TestCasesGenericOnFileFormat.EditorSettingsManagerTestCase
):
    """Test EditorSettingsManager with Json5Format"""

    def setUp(self):
        self.file_format = Json5FileFormat()
        return super().setUp()


class TestEditorSettingsFileYamlFormat(
    TestCasesGenericOnFileFormat.EditorSettingsFileTestCase
):
    """Test EditorSettingsFile with YamlFormat"""

    def setUp(self):
        self.file_format = YamlFileFormat()
        return super().setUp()


class TestEditorSettingsManagerYamlFormat(
    TestCasesGenericOnFileFormat.EditorSettingsManagerTestCase
):
    """Test EditorSettingsManager with YamlFormat"""

    def setUp(self):
        self.file_format = YamlFileFormat()
        return super().setUp()


if __name__ == '__main__':
    unittest.main()
