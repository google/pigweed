# Copyright 2023 The Pigweed Authors
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
"""Tests for the pw_build.bazel_query module."""

import json
import unittest

from unittest import mock
from unittest.mock import call

from pw_build.bazel_query import (
    BazelLabel,
    ParseError,
    BazelRule,
    BazelRepo,
)

# Test fixtures.


class MockCalls:
    def __init__(self, mock_run) -> None:
        self._calls: list[tuple] = []
        self._mock_run = mock_run
        self._side_effects: list[mock.MagicMock] = []
        self._mock_run.side_effect = self._side_effects

    def add(self, call_obj: tuple, retval: str) -> None:
        self._calls.extend([call_obj])
        self._side_effects.extend(
            [
                mock.MagicMock(**r)
                for r in [{'stdout.decode.return_value': retval}]
            ],
        )

    def check(self):
        self._mock_run.assert_has_calls(self._calls)


# Unit tests.


class TestBazelLabel(unittest.TestCase):
    """Tests for bazel_query.BazelLabel."""

    def test_label_with_repo_package_target(self):
        """Tests a label with a repo, package, and target."""
        label = BazelLabel.from_string(
            '@repo1//foo/bar:baz', repo_name='repo2', package='qux'
        )
        self.assertEqual(label.repo_name(), 'repo1')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo1//foo/bar:baz')

    def test_label_with_repo_package(self):
        """Tests a label with a repo and package."""
        label = BazelLabel.from_string(
            '@repo1//foo/bar', repo_name='repo2', package='qux'
        )
        self.assertEqual(label.repo_name(), 'repo1')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(str(label), '@repo1//foo/bar:bar')

    def test_label_with_repo_target(self):
        """Tests a label with a repo and target."""
        label = BazelLabel.from_string(
            '@repo1//:baz', repo_name='repo2', package='qux'
        )
        self.assertEqual(label.repo_name(), 'repo1')
        self.assertEqual(label.package(), '')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo1//:baz')

    def test_label_with_repo_only(self):
        """Tests a label with a repo only."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('@repo1', repo_name='repo2', package='qux')

    def test_label_with_package_target(self):
        """Tests a label with a package and target."""
        label = BazelLabel.from_string(
            '//foo/bar:baz', repo_name='repo2', package='qux'
        )
        self.assertEqual(label.repo_name(), 'repo2')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo2//foo/bar:baz')

    def test_label_with_package_only(self):
        """Tests a label with a package only."""
        label = BazelLabel.from_string(
            '//foo/bar', repo_name='repo2', package='qux'
        )
        self.assertEqual(label.repo_name(), 'repo2')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(str(label), '@repo2//foo/bar:bar')

    def test_label_with_target_only(self):
        """Tests a label with a target only."""
        label = BazelLabel.from_string(':baz', repo_name='repo2', package='qux')
        self.assertEqual(label.repo_name(), 'repo2')
        self.assertEqual(label.package(), 'qux')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo2//qux:baz')

    def test_label_with_none(self):
        """Tests an empty label."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('', repo_name='repo2', package='qux')

    def test_label_invalid_no_repo(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('//foo/bar:baz')

    def test_label_invalid_relative(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('../foo/bar:baz')

    def test_label_invalid_double_colon(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('//foo:bar:baz')


class TestBazelRule(unittest.TestCase):
    """Tests for bazel_query.BazelRule."""

    def test_rule_parse_invalid(self):
        """Test for parsing invalid rule attributes."""
        rule = BazelRule('kind', '@repo//package:target')
        with self.assertRaises(ParseError):
            rule.parse_attrs(
                json.loads(
                    '''[{
                        "name": "invalid_attr",
                        "type": "ESOTERIC",
                        "intValue": 0,
                        "stringValue": "false",
                        "explicitlySpecified": true,
                        "booleanValue": false
                    }]'''
                )
            )

    def test_rule_parse_boolean_unspecified(self):
        """Test parsing an unset boolean rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "bool_attr",
                    "type": "BOOLEAN",
                    "intValue": 0,
                    "stringValue": "false",
                    "explicitlySpecified": false,
                    "booleanValue": false
                }]'''
            )
        )
        self.assertFalse(rule.has_attr('bool_attr'))

    def test_rule_parse_boolean_false(self):
        """Tests parsing boolean rule attribute set to false."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "bool_attr",
                    "type": "BOOLEAN",
                    "intValue": 0,
                    "stringValue": "false",
                    "explicitlySpecified": true,
                    "booleanValue": false
                }]'''
            )
        )
        self.assertTrue(rule.has_attr('bool_attr'))
        self.assertFalse(rule.get_bool('bool_attr'))

    def test_rule_parse_boolean_true(self):
        """Tests parsing a boolean rule attribute set to true."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "bool_attr",
                    "type": "BOOLEAN",
                    "intValue": 1,
                    "stringValue": "true",
                    "explicitlySpecified": true,
                    "booleanValue": true
                }]'''
            )
        )
        self.assertTrue(rule.has_attr('bool_attr'))
        self.assertTrue(rule.get_bool('bool_attr'))

    def test_rule_parse_integer_unspecified(self):
        """Tests parsing an unset integer rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "int_attr",
                    "type": "INTEGER",
                    "intValue": 0,
                    "explicitlySpecified": false
                }]'''
            )
        )
        self.assertFalse(rule.has_attr('int_attr'))

    def test_rule_parse_integer(self):
        """Tests parsing an integer rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "int_attr",
                    "type": "INTEGER",
                    "intValue": 100,
                    "explicitlySpecified": true
                }]'''
            )
        )
        self.assertTrue(rule.has_attr('int_attr'))
        self.assertEqual(rule.get_int('int_attr'), 100)

    def test_rule_parse_string_unspecified(self):
        """Tests parsing an unset string rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "string_attr",
                    "type": "STRING",
                    "stringValue": "",
                    "explicitlySpecified": false
                }]'''
            )
        )
        self.assertFalse(rule.has_attr('string_attr'))

    def test_rule_parse_string(self):
        """Tests parsing a string rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "string_attr",
                    "type": "STRING",
                    "stringValue": "hello, world!",
                    "explicitlySpecified": true
                }]'''
            )
        )
        self.assertTrue(rule.has_attr('string_attr'))
        self.assertEqual(rule.get_str('string_attr'), 'hello, world!')

    def test_rule_parse_string_list_unspecified(self):
        """Tests parsing an unset string list rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "string_list_attr",
                    "type": "STRING_LIST",
                    "stringListValue": [],
                    "explicitlySpecified": false
                }]'''
            )
        )
        self.assertFalse(rule.has_attr('string_list_attr'))

    def test_rule_parse_string_list(self):
        """Tests parsing a string list rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "string_list_attr",
                    "type": "STRING_LIST",
                    "stringListValue": [ "hello", "world!" ],
                    "explicitlySpecified": true
                }]'''
            )
        )
        self.assertTrue(rule.has_attr('string_list_attr'))
        self.assertEqual(rule.get_list('string_list_attr'), ['hello', 'world!'])

    def test_rule_parse_label_list_unspecified(self):
        """Tests parsing an unset label list rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "label_list_attr",
                    "type": "LABEL_LIST",
                    "stringListValue": [],
                    "explicitlySpecified": false
                }]'''
            )
        )
        self.assertFalse(rule.has_attr('label_list_attr'))

    def test_rule_parse_label_list(self):
        """Tests parsing a label list rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "label_list_attr",
                    "type": "LABEL_LIST",
                    "stringListValue": [ "hello", "world!" ],
                    "explicitlySpecified": true
                }]'''
            )
        )
        self.assertTrue(rule.has_attr('label_list_attr'))
        self.assertEqual(rule.get_list('label_list_attr'), ['hello', 'world!'])

    def test_rule_parse_string_dict_unspecified(self):
        """Tests parsing an unset string dict rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "string_dict_attr",
                    "type": "LABEL_LIST",
                    "stringDictValue": [],
                    "explicitlySpecified": false
                }]'''
            )
        )
        self.assertFalse(rule.has_attr('string_dict_attr'))

    def test_rule_parse_string_dict(self):
        """Tests parsing a string dict rule attribute."""
        rule = BazelRule('kind', '@repo//package:target')
        rule.parse_attrs(
            json.loads(
                '''[{
                    "name": "string_dict_attr",
                    "type": "STRING_DICT",
                    "stringDictValue": [
                        {
                            "key": "foo",
                            "value": "hello"
                        },
                        {
                            "key": "bar",
                            "value": "world"
                        }
                    ],
                    "explicitlySpecified": true
                }]'''
            )
        )
        string_dict_attr = rule.get_dict('string_dict_attr')
        self.assertTrue(rule.has_attr('string_dict_attr'))
        self.assertEqual(string_dict_attr['foo'], 'hello')
        self.assertEqual(string_dict_attr['bar'], 'world')


class TestRepo(unittest.TestCase):
    """Test for bazel_query.Repo."""

    @mock.patch('subprocess.run')
    def test_repo_get_rules(self, mock_run):
        """Tests querying a repo for a rule."""
        mock_calls = MockCalls(mock_run)
        mock_calls.add(
            call(
                [
                    'bazelisk',
                    'cquery',
                    '@repo//pkg:target',
                    '--@repo//pkg:use_optional=True',
                    '--output=jsonproto',
                    '--noshow_progress',
                ],
                check=False,
                capture_output=True,
            ),
            json.dumps(
                {
                    'results': [
                        {
                            'target': {
                                'rule': {
                                    'name': '@repo//pkg:target',
                                    'ruleClass': 'cc_library',
                                    'attribute': [
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'hdrs',
                                            'type': 'string_list',
                                            'stringListValue': [
                                                'foo/include/bar.h'
                                            ],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'srcs',
                                            'type': 'string_list',
                                            'stringListValue': ['foo/bar.cc'],
                                        },
                                        {
                                            'name': 'additional_linker_inputs',
                                            'type': 'string_list',
                                            'stringListValue': ['implicit'],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'include_dirs',
                                            'type': 'string_list',
                                            'stringListValue': ['foo/include'],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'copts',
                                            'type': 'string_list',
                                            'stringListValue': [
                                                '-Wall',
                                                '-Werror',
                                            ],
                                        },
                                        {
                                            'explicitlySpecified': False,
                                            'name': 'linkopts',
                                            'type': 'string_list',
                                            'stringListValue': ['implicit'],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'defines',
                                            'type': 'string_list',
                                            'stringListValue': [
                                                '-DFOO',
                                                '-DBAR',
                                            ],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'local_defines',
                                            'type': 'string_list',
                                            'stringListValue': ['-DBAZ'],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'deps',
                                            'type': 'string_list',
                                            'stringListValue': [':baz'],
                                        },
                                        {
                                            'explicitlySpecified': True,
                                            'name': 'implementation_deps',
                                            'type': 'string_list',
                                            'stringListValue': [],
                                        },
                                    ],
                                }
                            }
                        }
                    ]
                }
            ),
        )

        repo = BazelRepo('repo')
        repo.generate = True
        repo.options = {'@repo//pkg:use_optional': True}
        labels = [BazelLabel.from_string('@repo//pkg:target')]
        rules = list(repo.get_rules(labels))
        rule = rules[0]
        self.assertEqual(rule.get_list('hdrs'), ['foo/include/bar.h'])
        self.assertEqual(rule.get_list('srcs'), ['foo/bar.cc'])
        self.assertEqual(rule.get_list('additional_linker_inputs'), [])
        self.assertEqual(rule.get_list('include_dirs'), ['foo/include'])
        self.assertEqual(rule.get_list('copts'), ['-Wall', '-Werror'])
        self.assertEqual(rule.get_list('linkopts'), [])
        self.assertEqual(rule.get_list('defines'), ['-DFOO', '-DBAR'])
        self.assertEqual(rule.get_list('local_defines'), ['-DBAZ'])
        self.assertEqual(rule.get_list('deps'), [':baz'])
        self.assertEqual(rule.get_list('implementation_deps'), [])

        # Rules are cached, so a second call doesn't invoke Bazel again.
        repo.get_rules(labels)

        mock_calls.check()

    @mock.patch('subprocess.run')
    def test_repo_get_rule_no_generate(self, mock_run):
        """Tests querying a repo for a rule."""
        repo = BazelRepo('repo')
        repo.generate = False
        labels = [BazelLabel.from_string('@repo//pkg:target')]
        rules = list(repo.get_rules(labels))
        self.assertEqual(rules, [])
        self.assertFalse(mock_run.called)

    @mock.patch('subprocess.run')
    def test_repo_revision(self, mock_run):
        """Tests querying a repo for its git revision."""
        mock_calls = MockCalls(mock_run)
        mock_calls.add(
            call(
                ['bazelisk', 'mod', 'show_repo', 'repo', '--noshow_progress'],
                check=False,
                capture_output=True,
            ),
            '''
http_archive(
    urls = ["https://github.com/fake/repo/releases/download/tag1/repo.tar.gz"],
)
''',
        )
        mock_calls.add(
            call(
                [
                    'git',
                    'ls-remote',
                    'https://github.com/fake/repo',
                    '-t',
                    'tag1',
                ],
                check=True,
                capture_output=True,
            ),
            'deadbeef\trefs/tags/tag1',
        )

        repo = BazelRepo('repo')
        self.assertEqual(repo.revision(), 'deadbeef')

        mock_calls.check()


if __name__ == '__main__':
    unittest.main()
