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

from collections import deque
from typing import Any, Deque
from unittest import mock

from pw_build.bazel_query import (
    ParseError,
    BazelLabel,
    BazelRule,
    BazelWorkspace,
)

# Test fixtures.


class MockWorkspace:
    """Helper class for mocking subprocesses run by BazelWorkspace."""

    def __init__(self, source_dir: str) -> None:
        """Creates a workspace mock helper."""
        self._args_list: Deque[list[Any]] = deque()
        self._kwargs_list: Deque[dict[str, Any]] = deque()
        self._results: list[dict[str, str]] = []
        self._source_dir = source_dir

    def source_dir(self) -> str:
        """Returns the source directory commands would be run in."""
        return self._source_dir

    def add_call(self, *args: str, result=None) -> None:
        """Registers an expected subprocess call."""
        self._args_list.append(list(args))
        self._results.append({'stdout.decode.return_value': result})

    def finalize(self, mock_run) -> None:
        """Add the registered call results to a subprocess mock."""
        mock_run.side_effect = [
            mock.MagicMock(**attr) for attr in self._results
        ]

    def pop_args(self) -> list[Any]:
        """Returns the next set of saved args."""
        return self._args_list.popleft()

    def num_args(self) -> int:
        """Returns the number of remaining saved args."""
        return len(self._args_list)


# Unit tests


class TestBazelLabel(unittest.TestCase):
    """Tests for bazel_query.BazelLabel."""

    def test_label_with_repo_package_target(self):
        """Tests a label with a repo, package, and target."""
        label = BazelLabel('@repo1//foo/bar:baz', repo='repo2', package='qux')
        self.assertEqual(label.repo(), 'repo1')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.target(), 'baz')
        self.assertEqual(str(label), '@repo1//foo/bar:baz')

    def test_label_with_repo_package(self):
        """Tests a label with a repo and package."""
        label = BazelLabel('@repo1//foo/bar', repo='repo2', package='qux')
        self.assertEqual(label.repo(), 'repo1')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.target(), 'bar')
        self.assertEqual(str(label), '@repo1//foo/bar:bar')

    def test_label_with_repo_target(self):
        """Tests a label with a repo and target."""
        label = BazelLabel('@repo1//:baz', repo='repo2', package='qux')
        self.assertEqual(label.repo(), 'repo1')
        self.assertEqual(label.package(), '')
        self.assertEqual(label.target(), 'baz')
        self.assertEqual(str(label), '@repo1//:baz')

    def test_label_with_repo_only(self):
        """Tests a label with a repo only."""
        with self.assertRaises(ParseError):
            BazelLabel('@repo1', repo='repo2', package='qux')

    def test_label_with_package_target(self):
        """Tests a label with a package and target."""
        label = BazelLabel('//foo/bar:baz', repo='repo2', package='qux')
        self.assertEqual(label.repo(), 'repo2')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.target(), 'baz')
        self.assertEqual(str(label), '@repo2//foo/bar:baz')

    def test_label_with_package_only(self):
        """Tests a label with a package only."""
        label = BazelLabel('//foo/bar', repo='repo2', package='qux')
        self.assertEqual(label.repo(), 'repo2')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.target(), 'bar')
        self.assertEqual(str(label), '@repo2//foo/bar:bar')

    def test_label_with_target_only(self):
        """Tests a label with a target only."""
        label = BazelLabel(':baz', repo='repo2', package='qux')
        self.assertEqual(label.repo(), 'repo2')
        self.assertEqual(label.package(), 'qux')
        self.assertEqual(label.target(), 'baz')
        self.assertEqual(str(label), '@repo2//qux:baz')

    def test_label_with_none(self):
        """Tests an empty label."""
        with self.assertRaises(ParseError):
            BazelLabel('', repo='repo2', package='qux')

    def test_label_invalid_no_repo(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(AssertionError):
            BazelLabel('//foo/bar:baz')

    def test_label_invalid_relative(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel('../foo/bar:baz')

    def test_label_invalid_double_colon(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel('//foo:bar:baz')


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


class TestWorkspace(unittest.TestCase):
    """Test for bazel_query.Workspace."""

    def setUp(self) -> None:
        self.mock = MockWorkspace('path/to/repo')

    def verify_mock(self, mock_run) -> None:
        """Asserts that the calls to the mock object match those registered."""
        for mocked_call in mock_run.call_args_list:
            self.assertEqual(mocked_call.args[0], self.mock.pop_args())
            self.assertEqual(mocked_call.kwargs['cwd'], 'path/to/repo')
            self.assertTrue(mocked_call.kwargs['capture_output'])
        self.assertEqual(self.mock.num_args(), 0)

    @mock.patch('subprocess.run')
    def test_workspace_get_http_archives_no_generate(self, mock_run):
        """Tests querying a workspace for its external dependencies."""
        self.mock.add_call('git', 'fetch')
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        workspace.generate = False
        deps = list(workspace.get_http_archives())
        self.assertEqual(deps, [])
        self.verify_mock(mock_run)

    @mock.patch('subprocess.run')
    def test_workspace_get_http_archives(self, mock_run):
        """Tests querying a workspace for its external dependencies."""
        self.mock.add_call('git', 'fetch')
        rules = [
            {
                'type': 'RULE',
                'rule': {
                    'name': '//external:foo',
                    'ruleClass': 'http_archive',
                    'attribute': [
                        {
                            'name': 'url',
                            'type': 'STRING',
                            'explicitlySpecified': True,
                            'stringValue': 'http://src/deadbeef.tgz',
                        }
                    ],
                },
            },
            {
                'type': 'RULE',
                'rule': {
                    'name': '//external:bar',
                    'ruleClass': 'http_archive',
                    'attribute': [
                        {
                            'name': 'urls',
                            'type': 'STRING_LIST',
                            'explicitlySpecified': True,
                            'stringListValue': ['http://src/feedface.zip'],
                        }
                    ],
                },
            },
        ]
        results = [json.dumps(rule) for rule in rules]
        self.mock.add_call(
            'bazel',
            'query',
            'kind(http_archive, //external:*)',
            '--output=streamed_jsonproto',
            '--noshow_progress',
            result='\n'.join(results),
        )
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        (foo_rule, bar_rule) = list(workspace.get_http_archives())
        self.assertEqual(foo_rule.get_str('url'), 'http://src/deadbeef.tgz')
        self.assertEqual(
            bar_rule.get_list('urls'),
            [
                'http://src/feedface.zip',
            ],
        )
        self.verify_mock(mock_run)

    @mock.patch('subprocess.run')
    def test_workspace_get_rules(self, mock_run):
        """Tests querying a workspace for a rule."""
        self.mock.add_call('git', 'fetch')
        rule_data = {
            'results': [
                {
                    'target': {
                        'rule': {
                            'name': '//pkg:target',
                            'ruleClass': 'cc_library',
                            'attribute': [
                                {
                                    'explicitlySpecified': True,
                                    'name': 'hdrs',
                                    'type': 'string_list',
                                    'stringListValue': ['foo/include/bar.h'],
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
                                    'stringListValue': ['-Wall', '-Werror'],
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
                                    'stringListValue': ['-DFILTERED', '-DKEPT'],
                                },
                                {
                                    'explicitlySpecified': True,
                                    'name': 'local_defines',
                                    'type': 'string_list',
                                    'stringListValue': ['-DALSO_FILTERED'],
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
        self.mock.add_call(
            'bazel',
            'cquery',
            '//pkg:target',
            '--@repo//pkg:use_optional=True',
            '--output=jsonproto',
            '--noshow_progress',
            result=json.dumps(rule_data),
        )
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        workspace.defaults = {
            'defines': ['-DFILTERED'],
            'local_defines': ['-DALSO_FILTERED'],
        }
        workspace.options = {'@repo//pkg:use_optional': True}
        labels = [BazelLabel('@repo//pkg:target')]
        rules = list(workspace.get_rules(labels))
        rule = rules[0]
        self.assertEqual(rule.get_list('hdrs'), ['foo/include/bar.h'])
        self.assertEqual(rule.get_list('srcs'), ['foo/bar.cc'])
        self.assertEqual(rule.get_list('additional_linker_inputs'), [])
        self.assertEqual(rule.get_list('include_dirs'), ['foo/include'])
        self.assertEqual(rule.get_list('copts'), ['-Wall', '-Werror'])
        self.assertEqual(rule.get_list('linkopts'), [])
        self.assertEqual(rule.get_list('defines'), ['-DKEPT'])
        self.assertEqual(rule.get_list('local_defines'), [])
        self.assertEqual(rule.get_list('deps'), [':baz'])
        self.assertEqual(rule.get_list('implementation_deps'), [])

        # Rules are cached, so a second call doesn't invoke Bazel again.
        rule = workspace.get_rules(labels)
        self.verify_mock(mock_run)

    @mock.patch('subprocess.run')
    def test_workspace_get_rule_no_generate(self, mock_run):
        """Tests querying a workspace for a rule."""
        self.mock.add_call('git', 'fetch')
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        workspace.generate = False
        labels = [BazelLabel('@repo//pkg:target')]
        rules = list(workspace.get_rules(labels))
        self.assertEqual(rules, [])
        self.verify_mock(mock_run)

    @mock.patch('subprocess.run')
    def test_workspace_revision(self, mock_run):
        """Tests querying a workspace for its git revision."""
        self.mock.add_call('git', 'fetch')
        self.mock.add_call('git', 'rev-parse', 'HEAD', result='deadbeef')
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        self.assertEqual(workspace.revision(), 'deadbeef')
        self.verify_mock(mock_run)

    @mock.patch('subprocess.run')
    def test_workspace_timestamp(self, mock_run):
        """Tests querying a workspace for its commit timestamp."""
        self.mock.add_call('git', 'fetch')
        self.mock.add_call(
            'git', 'show', '--no-patch', '--format=%ci', 'HEAD', result='0123'
        )
        self.mock.add_call(
            'git',
            'show',
            '--no-patch',
            '--format=%ci',
            'deadbeef',
            result='4567',
        )
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        self.assertEqual(workspace.timestamp('HEAD'), '0123')
        self.assertEqual(workspace.timestamp('deadbeef'), '4567')
        self.verify_mock(mock_run)

    @mock.patch('subprocess.run')
    def test_workspace_url(self, mock_run):
        """Tests querying a workspace for its git URL."""
        self.mock.add_call('git', 'fetch')
        self.mock.add_call(
            'git', 'remote', 'get-url', 'origin', result='http://foo/bar.git'
        )
        self.mock.finalize(mock_run)
        workspace = BazelWorkspace('@repo', self.mock.source_dir())
        self.assertEqual(workspace.url(), 'http://foo/bar.git')
        self.verify_mock(mock_run)


if __name__ == '__main__':
    unittest.main()
