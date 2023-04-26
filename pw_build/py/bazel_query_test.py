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
from tempfile import TemporaryDirectory
from pw_build.bazel_query import ParseError, BazelRule, BazelWorkspace


class TestBazelRule(unittest.TestCase):
    """Tests for bazel_query.Rule."""

    def test_rule_top_level(self):
        """Tests a top-level rule with no package name."""
        rule = BazelRule('//:no-package', 'custom-type')
        self.assertEqual(rule.package(), '')

    def test_rule_with_label(self):
        """Tests a rule with a package and target name."""
        rule = BazelRule('//foo:target', 'custom-type')
        self.assertEqual(rule.package(), 'foo')
        self.assertEqual(rule.label(), '//foo:target')

    def test_rule_in_subdirectory(self):
        """Tests a rule in a subdirectory."""
        rule = BazelRule('//foo:bar/target', 'custom-type')
        self.assertEqual(rule.package(), 'foo')
        self.assertEqual(rule.label(), '//foo:bar/target')

    def test_rule_in_subpackage(self):
        """Tests a rule in a subpackage."""
        rule = BazelRule('//foo/bar:target', 'custom-type')
        self.assertEqual(rule.package(), 'foo/bar')
        self.assertEqual(rule.label(), '//foo/bar:target')

    def test_rule_no_target(self):
        """Tests a rule with only a package name."""
        rule = BazelRule('//foo/bar', 'custom-type')
        self.assertEqual(rule.package(), 'foo/bar')
        self.assertEqual(rule.label(), '//foo/bar:bar')

    def test_rule_invalid_relative(self):
        """Tests a rule with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelRule('../foo/bar:target', 'custom-type')

    def test_rule_invalid_double_colon(self):
        """Tests a rule with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelRule('//foo:bar:target', 'custom-type')

    def test_rule_parse_invalid(self):
        """Test for parsing invalid rule attributes."""
        rule = BazelRule('//package:target', 'kind')
        with self.assertRaises(ParseError):
            rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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
        rule = BazelRule('//package:target', 'kind')
        rule.parse(
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

    @mock.patch('pw_build.bazel_query.BazelWorkspace._run')
    def test_workspace_get_rules(self, mock_run):
        """Tests querying a workspace for Bazel rules."""
        packages = '\n'.join(['foo/pkg1', 'bar/pkg2'])

        foo_default_visibility = '''
<query version="2">
    <source-file name="//foo/pkg1:BUILD.bazel">
        <visibility-label name="//visibility:public"/>
    </source-file>
</query>'''

        bar_default_visibility = '''
<query version="2">
    <source-file name="//bar/pkg2:BUILD.bazel">
        <visibility-label name="//visibility:private"/>
    </source-file>
</query>'''

        some_kind_rules = '''
{
  "results": [
    {
      "target": {
        "type": "RULE",
        "rule": {
          "name": "//foo/pkg1:rule1",
          "ruleClass": "some_kind",
          "attribute": []
        }
      }
    },
    {
      "target": {
        "type": "RULE",
        "rule": {
          "name": "//bar/pkg2:rule2",
          "ruleClass": "some_kind",
          "attribute": []
        }
      }
    }
  ]
}'''
        mock_run.side_effect = [
            packages,
            foo_default_visibility,
            bar_default_visibility,
            some_kind_rules,
        ]

        with TemporaryDirectory() as tmp:
            workspace = BazelWorkspace(tmp)
            rules = list(workspace.get_rules('some_kind'))
            actual = [r.label() for r in rules]
            self.assertEqual(actual, ['//foo/pkg1:rule1', '//bar/pkg2:rule2'])

    @mock.patch('subprocess.run')
    def test_revision(self, mock_run):
        """Tests writing an OWNERS file."""
        attrs = {'stdout.decode.return_value': 'fake-hash'}
        mock_run.return_value = mock.MagicMock(**attrs)

        with TemporaryDirectory() as tmp:
            workspace = BazelWorkspace(tmp)
            self.assertEqual(workspace.revision(), 'fake-hash')
            args, kwargs = mock_run.call_args
            self.assertEqual(*args, ['git', 'rev-parse', 'HEAD'])
            self.assertEqual(kwargs['cwd'], tmp)

    @mock.patch('subprocess.run')
    def test_url(self, mock_run):
        """Tests writing an OWNERS file."""
        attrs = {'stdout.decode.return_value': 'https://repohub.com/repo.git'}
        mock_run.return_value = mock.MagicMock(**attrs)

        with TemporaryDirectory() as tmp:
            workspace = BazelWorkspace(tmp)
            self.assertEqual(workspace.url(), 'https://repohub.com/repo.git')
            args, kwargs = mock_run.call_args
            self.assertEqual(*args, ['git', 'remote', 'get-url', 'origin'])
            self.assertEqual(kwargs['cwd'], tmp)


if __name__ == '__main__':
    unittest.main()
