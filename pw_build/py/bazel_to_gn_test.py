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
"""Tests for the pw_build.bazel_to_gn module."""

import json
import unittest

from io import StringIO

from unittest import mock
from unittest.mock import call

from pw_build.bazel_to_gn import BazelToGnConverter

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


# Simulated out/args.gn file contents.
MODULE_BAZEL = '''module(
    name = "pigweed",
    version = "0.0.1",
)

register_execution_platforms("@local_config_platform//:host")

bazel_dep(name = "foo", version = "1.2.3")
bazel_dep(name = "bar", version = "2025-02-17")
bazel_dep(name = "baz", version = "20250217.2")
'''

# Simulated //third_party.../bazel_to_gn.json file contents.
FOO_B2G_JSON = '''{
  "generate": true,
  "targets": [ "@foo//package:name" ]
}'''
BAR_B2G_JSON = '''{
  "generate": true,
  "options": {
    "@bar//package:my_flag": true
  },
  "targets": [ "@bar//bar/pkg:bar_target1" ]
}'''

# Simulated 'bazelisk cquery ...' results.
FOO_RULE_JSON = '''{
  "results": [
    {
      "target": {
        "rule": {
          "name": "@foo//package:target",
          "ruleClass": "cc_library",
          "attribute": [
            {
              "explicitlySpecified": true,
              "name": "hdrs",
              "type": "label_list",
              "stringListValue": [ "//include:foo.h" ]
            },
            {
              "explicitlySpecified": true,
              "name": "srcs",
              "type": "label_list",
              "stringListValue": [ "//src:foo.cc" ]
            },
            {
              "explicitlySpecified": true,
              "name": "additional_linker_inputs",
              "type": "label_list",
              "stringListValue": [ "//data:input" ]
            },
            {
              "explicitlySpecified": true,
              "name": "includes",
              "type": "string_list",
              "stringListValue": [ "include" ]
            },
            {
              "explicitlySpecified": true,
              "name": "copts",
              "type": "string_list",
              "stringListValue": [ "-cflag" ]
            },
            {
              "explicitlySpecified": true,
              "name": "linkopts",
              "type": "string_list",
              "stringListValue": [ "-ldflag" ]
            },
            {
              "explicitlySpecified": true,
              "name": "defines",
              "type": "string_list",
              "stringListValue": [ "DEFINE" ]
            },
            {
              "explicitlySpecified": true,
              "name": "local_defines",
              "type": "string_list",
              "stringListValue": [ "LOCAL_DEFINE" ]
            },
            {
              "explicitlySpecified": true,
              "name": "deps",
              "type": "label_list",
              "stringListValue": [
                "@bar//bar/pkg:bar_target1",
                "@bar//bar/pkg:bar_target2"
              ]
            },
            {
              "explicitlySpecified": true,
              "name": "implementation_deps",
              "type": "label_list",
              "stringListValue": [ "@baz//baz/pkg:baz_target" ]
            }
          ]
        }
      }
    }
  ]
}
'''

# Unit tests.


class TestBazelToGnConverter(unittest.TestCase):
    """Tests for bazel_to_gn.BazelToGnConverter."""

    def test_parse_bazel_to_gn(self):
        """Tests loading a repo from a bazel_to_gn.json file."""
        b2g = BazelToGnConverter()
        b2g.load_modules(StringIO(MODULE_BAZEL))
        b2g.parse_bazel_to_gn('foo', StringIO(FOO_B2G_JSON))
        b2g.parse_bazel_to_gn('bar', StringIO(BAR_B2G_JSON))
        self.assertEqual(
            set(b2g.repo_names()), set(['pigweed', 'foo', 'bar', 'baz'])
        )

    def test_get_initial_targets(self):
        """Tests adding initial targets to the pending queue."""
        b2g = BazelToGnConverter()
        b2g.load_modules(StringIO(MODULE_BAZEL))
        b2g.parse_bazel_to_gn('foo', StringIO(FOO_B2G_JSON))
        targets = b2g.get_initial_targets('foo')
        json_targets = json.loads(FOO_B2G_JSON)['targets']
        self.assertEqual(len(targets), len(json_targets))
        self.assertEqual(b2g.num_loaded(), 1)

    @mock.patch('subprocess.run')
    def test_load_rules(self, mock_run):
        """Tests loading a rule from a Bazel repo."""
        mock_calls = MockCalls(mock_run)
        mock_calls.add(
            call(
                [
                    'bazelisk',
                    'cquery',
                    '@foo//package:name',
                    '--output=jsonproto',
                    '--noshow_progress',
                ],
                check=False,
                capture_output=True,
            ),
            FOO_RULE_JSON,
        )

        b2g = BazelToGnConverter()
        b2g.load_modules(StringIO(MODULE_BAZEL))
        b2g.parse_bazel_to_gn('foo', StringIO(FOO_B2G_JSON))
        labels = b2g.get_initial_targets('foo')
        rule = list(b2g.load_rules(labels))[0]
        self.assertEqual(
            rule.get_list('deps'),
            [
                '@bar//bar/pkg:bar_target1',
                '@bar//bar/pkg:bar_target2',
            ],
        )
        self.assertEqual(
            rule.get_list('implementation_deps'),
            [
                '@baz//baz/pkg:baz_target',
            ],
        )
        self.assertEqual(b2g.num_loaded(), 1)
        mock_calls.check()

    @mock.patch('subprocess.run')
    def test_convert_rule(self, mock_run):
        """Tests converting a Bazel rule into a GN target."""
        mock_calls = MockCalls(mock_run)
        mock_calls.add(
            call(
                [
                    'bazelisk',
                    'cquery',
                    '@foo//package:name',
                    '--output=jsonproto',
                    '--noshow_progress',
                ],
                check=False,
                capture_output=True,
            ),
            FOO_RULE_JSON,
        )

        b2g = BazelToGnConverter()
        b2g.load_modules(StringIO(MODULE_BAZEL))
        b2g.parse_bazel_to_gn('foo', StringIO(FOO_B2G_JSON))
        b2g.parse_bazel_to_gn('bar', StringIO(BAR_B2G_JSON))
        labels = b2g.get_initial_targets('foo')
        rule = list(b2g.load_rules(labels))[0]
        gn_target = b2g.convert_rule(rule)
        self.assertEqual(gn_target.attrs['cflags'], ['-cflag'])
        self.assertEqual(gn_target.attrs['defines'], ['LOCAL_DEFINE'])
        self.assertEqual(
            gn_target.attrs['deps'],
            ['$pw_external_baz/baz/pkg:baz_target'],
        )
        self.assertEqual(
            gn_target.attrs['include_dirs'], ['$dir_pw_third_party_foo/include']
        )
        self.assertEqual(
            gn_target.attrs['inputs'], ['$dir_pw_third_party_foo/data/input']
        )
        self.assertEqual(gn_target.attrs['ldflags'], ['-ldflag'])
        self.assertEqual(
            gn_target.attrs['public'], ['$dir_pw_third_party_foo/include/foo.h']
        )
        self.assertEqual(gn_target.attrs['public_defines'], ['DEFINE'])
        self.assertEqual(
            gn_target.attrs['public_deps'],
            [
                '$pw_external_bar/bar/pkg:bar_target1',
                '$pw_external_bar/bar/pkg:bar_target2',
            ],
        )
        self.assertEqual(
            gn_target.attrs['sources'], ['$dir_pw_third_party_foo/src/foo.cc']
        )

        mock_calls.check()

    @mock.patch('subprocess.run')
    def test_update_pw_package(self, mock_run):
        """Tests updating the pw_package file."""
        mock_calls = MockCalls(mock_run)
        mock_calls.add(
            call(
                ['bazelisk', 'mod', 'show_repo', 'foo', '--noshow_progress'],
                check=False,
                capture_output=True,
            ),
            '''
http_archive(
    urls = ["https://github.com/test/foo/releases/download/some-tag/foo.tgz"],
)
''',
        )
        mock_calls.add(
            call(
                [
                    'git',
                    'ls-remote',
                    'https://github.com/test/foo',
                    '-t',
                    'some-tag',
                ],
                check=True,
                capture_output=True,
            ),
            'feedface\trefs/tags/some-tag',
        )

        b2g = BazelToGnConverter()
        b2g.load_modules(StringIO(MODULE_BAZEL))
        b2g.parse_bazel_to_gn('foo', StringIO(FOO_B2G_JSON))
        contents = '''some_python_call(
    name='foo',
    commit='cafef00d',
    **kwargs,
)
'''
        inputs = contents.split('\n')
        outputs = list(b2g.update_pw_package('foo', inputs))

        self.assertEqual(outputs[0:2], inputs[0:2])
        self.assertEqual(outputs[3], inputs[3].replace('cafef00d', 'some-tag'))
        self.assertEqual(outputs[4:-1], inputs[4:])

        mock_calls.check()

    @mock.patch('subprocess.run')
    def test_get_imports(self, mock_run):
        """Tests getting the GNI files needed for a GN target."""
        mock_calls = MockCalls(mock_run)
        mock_calls.add(
            call(
                [
                    'bazelisk',
                    'cquery',
                    '@foo//package:name',
                    '--output=jsonproto',
                    '--noshow_progress',
                ],
                check=False,
                capture_output=True,
            ),
            FOO_RULE_JSON,
        )

        b2g = BazelToGnConverter()
        b2g.load_modules(StringIO(MODULE_BAZEL))
        b2g.parse_bazel_to_gn('foo', StringIO(FOO_B2G_JSON))
        b2g.parse_bazel_to_gn('bar', StringIO(BAR_B2G_JSON))
        labels = b2g.get_initial_targets('foo')
        rule = list(b2g.load_rules(labels))[0]
        gn_target = b2g.convert_rule(rule)
        imports = set(b2g.get_imports(gn_target))
        self.assertEqual(imports, {'$pw_external_foo/foo.gni'})

        mock_calls.check()


if __name__ == '__main__':
    unittest.main()
