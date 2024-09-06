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
from pathlib import PurePath

from unittest import mock

from pw_build.bazel_to_gn import BazelToGnConverter

# Test fixtures.

PW_ROOT = '/path/to/pigweed'
FOO_SOURCE_DIR = '/path/to/foo'
BAR_SOURCE_DIR = '../relative/path/to/bar'
BAZ_SOURCE_DIR = '/path/to/baz'

# Simulated out/args.gn file contents.
ARGS_GN = f'''dir_pw_third_party_foo = "{FOO_SOURCE_DIR}"
pw_log_BACKEND = "$dir_pw_log_basic"
pw_unit_test_MAIN = "$dir_pw_unit_test:logging_main"
dir_pw_third_party_bar = "{BAR_SOURCE_DIR}"
pw_unit_test_GOOGLETEST_BACKEND == "$dir_pw_third_party/googletest"
pw_unit_test_MAIN == "$dir_pw_third_party/googletest:gmock_main
dir_pw_third_party_baz = "{BAZ_SOURCE_DIR}"'''

# Simulated Bazel repo names.
FOO_REPO = 'dev_pigweed_foo'
BAR_REPO = 'dev_pigweed_bar'
BAZ_REPO = 'dev_pigweed_baz'

# Simulated //third_party.../bazel_to_gn.json file contents.
FOO_B2G_JSON = f'''{{
  "repo": "{FOO_REPO}",
  "targets": [ "//package:target" ]
}}'''
BAR_B2G_JSON = f'''{{
  "repo": "{BAR_REPO}",
  "options": {{
    "//package:my_flag": true
  }},
  "targets": [ "//bar/pkg:bar_target1" ]
}}'''
BAZ_B2G_JSON = f'''{{
  "repo": "{BAZ_REPO}",
  "generate": false
}}'''

# Simulated 'bazel cquery ...' results.
FOO_RULE_JSON = f'''{{
  "results": [
    {{
      "target": {{
        "rule": {{
          "name": "//package:target",
          "ruleClass": "cc_library",
          "attribute": [
            {{
              "explicitlySpecified": true,
              "name": "hdrs",
              "type": "label_list",
              "stringListValue": [ "//include:foo.h" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "srcs",
              "type": "label_list",
              "stringListValue": [ "//src:foo.cc" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "additional_linker_inputs",
              "type": "label_list",
              "stringListValue": [ "//data:input" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "includes",
              "type": "string_list",
              "stringListValue": [ "include" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "copts",
              "type": "string_list",
              "stringListValue": [ "-cflag" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "linkopts",
              "type": "string_list",
              "stringListValue": [ "-ldflag" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "defines",
              "type": "string_list",
              "stringListValue": [ "DEFINE" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "local_defines",
              "type": "string_list",
              "stringListValue": [ "LOCAL_DEFINE" ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "deps",
              "type": "label_list",
              "stringListValue": [
                "@{BAR_REPO}//bar/pkg:bar_target1",
                "@{BAR_REPO}//bar/pkg:bar_target2"
              ]
            }},
            {{
              "explicitlySpecified": true,
              "name": "implementation_deps",
              "type": "label_list",
              "stringListValue": [ "@{BAZ_REPO}//baz/pkg:baz_target" ]
            }}
          ]
        }}
      }}
    }}
  ]
}}
'''
BAR_RULE_JSON = '''
{
  "results": [
    {
      "target": {
        "rule": {
          "name": "//bar/pkg:bar_target1",
          "ruleClass": "cc_library",
          "attribute": [
            {
              "explicitlySpecified": true,
              "name": "defines",
              "type": "string_list",
              "stringListValue": [ "FILTERED", "KEPT" ]
            }
          ]
        }
      }
    }
  ]
}
'''

# Simulated Bazel WORKSPACE file for Pigweed.
# Keep this in sync with PW_EXTERNAL_DEPS below.
PW_WORKSPACE = f'''
http_archive(
    name = "{FOO_REPO}",
    strip_prefix = "foo-feedface",
    url = "http://localhost:9000/feedface.tgz",
)

http_archive(
    name = "{BAR_REPO}",
    strip_prefix = "bar-v1.0",
    urls = ["http://localhost:9000/bar/v1.0.tgz"],
)

http_archive(
    name = "{BAZ_REPO}",
    strip_prefix = "baz-v1.5",
    url = "http://localhost:9000/baz/v1.5.zip",
)

http_archive(
    name = "other",
    strip_prefix = "other-v2.0",
    url = "http://localhost:9000/other/v2.0.zip",
)

another_rule(
    # aribtrary contents
)
'''

# Simulated 'bazel query //external:*' results for com_google_pigweed.
# Keep this in sync with PW_WORKSPACE above.
PW_EXTERNAL_DEPS = '\n'.join(
    [
        json.dumps(
            {
                'type': 'RULE',
                'rule': {
                    'name': f'//external:{FOO_REPO}',
                    'ruleClass': 'http_archive',
                    'attribute': [
                        {
                            'name': 'strip_prefix',
                            'explicitlySpecified': True,
                            "type": "string",
                            'stringValue': 'foo-feedface',
                        },
                        {
                            'name': 'url',
                            'explicitlySpecified': True,
                            "type": "string",
                            'stringValue': 'http://localhost:9000/feedface.tgz',
                        },
                    ],
                },
            }
        ),
        json.dumps(
            {
                'type': 'RULE',
                'rule': {
                    'name': f'//external:{BAR_REPO}',
                    'ruleClass': 'http_archive',
                    'attribute': [
                        {
                            'name': 'strip_prefix',
                            'explicitlySpecified': True,
                            "type": "string",
                            'stringValue': 'bar-v1.0',
                        },
                        {
                            'name': 'urls',
                            'explicitlySpecified': True,
                            "type": "string_list",
                            'stringListValue': [
                                'http://localhost:9000/bar/v1.0.tgz'
                            ],
                        },
                    ],
                },
            }
        ),
    ]
)
# Simulated 'bazel query //external:*' results for dev_pigweed_foo.
FOO_EXTERNAL_DEPS = '\n'.join(
    [
        json.dumps(
            {
                'type': 'RULE',
                'rule': {
                    'name': f'//external:{BAR_REPO}',
                    'ruleClass': 'http_archive',
                    'attribute': [
                        {
                            'name': 'strip_prefix',
                            'explicitlySpecified': True,
                            "type": "string",
                            'stringValue': 'bar-v2.0',
                        },
                        {
                            'name': 'urls',
                            'explicitlySpecified': True,
                            "type": "string_list",
                            'stringListValue': [
                                'http://localhost:9000/bar/v2.0.tgz'
                            ],
                        },
                    ],
                },
            }
        ),
        json.dumps(
            {
                'type': 'RULE',
                'rule': {
                    'name': f'//external:{BAZ_REPO}',
                    'ruleClass': 'http_archive',
                    'attribute': [
                        {
                            'name': 'url',
                            'explicitlySpecified': True,
                            "type": "string",
                            'stringValue': 'http://localhost:9000/baz/v1.5.tgz',
                        }
                    ],
                },
            }
        ),
    ]
)
# Unit tests.


class TestBazelToGnConverter(unittest.TestCase):
    """Tests for bazel_to_gn.BazelToGnConverter."""

    def test_parse_args_gn(self):
        """Tests parsing args.gn."""
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        self.assertEqual(b2g.get_source_dir('foo'), PurePath(FOO_SOURCE_DIR))
        self.assertEqual(b2g.get_source_dir('bar'), PurePath(BAR_SOURCE_DIR))
        self.assertEqual(b2g.get_source_dir('baz'), PurePath(BAZ_SOURCE_DIR))

    @mock.patch('subprocess.run')
    def test_load_workspace(self, _):
        """Tests loading a workspace from a bazel_to_gn.json file."""
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
        b2g.load_workspace('bar', StringIO(BAR_B2G_JSON))
        b2g.load_workspace('baz', StringIO(BAZ_B2G_JSON))
        self.assertEqual(b2g.get_name(repo=FOO_REPO), 'foo')
        self.assertEqual(b2g.get_name(repo=BAR_REPO), 'bar')
        self.assertEqual(b2g.get_name(repo=BAZ_REPO), 'baz')

    @mock.patch('subprocess.run')
    def test_get_initial_targets(self, _):
        """Tests adding initial targets to the pending queue."""
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
        targets = b2g.get_initial_targets('foo')
        json_targets = json.loads(FOO_B2G_JSON)['targets']
        self.assertEqual(len(targets), len(json_targets))
        self.assertEqual(b2g.num_loaded(), 1)

    @mock.patch('subprocess.run')
    def test_load_rules(self, mock_run):
        """Tests loading a rule from a Bazel workspace."""
        mock_run.side_effect = [
            mock.MagicMock(**retval)
            for retval in [
                {'stdout.decode.return_value': ''},  # foo: git fetch
                {'stdout.decode.return_value': FOO_RULE_JSON},
            ]
        ]
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
        labels = b2g.get_initial_targets('foo')
        rule = list(b2g.load_rules(labels))[0]
        self.assertEqual(
            rule.get_list('deps'),
            [
                f'@{BAR_REPO}//bar/pkg:bar_target1',
                f'@{BAR_REPO}//bar/pkg:bar_target2',
            ],
        )
        self.assertEqual(
            rule.get_list('implementation_deps'),
            [
                f'@{BAZ_REPO}//baz/pkg:baz_target',
            ],
        )
        self.assertEqual(b2g.num_loaded(), 1)

    @mock.patch('subprocess.run')
    def test_convert_rule(self, mock_run):
        """Tests converting a Bazel rule into a GN target."""
        mock_run.side_effect = [
            mock.MagicMock(**retval)
            for retval in [
                {'stdout.decode.return_value': ''},  # foo: git fetch
                {'stdout.decode.return_value': ''},  # bar: git fetch
                {'stdout.decode.return_value': ''},  # baz: git fetch
                {'stdout.decode.return_value': FOO_RULE_JSON},
            ]
        ]
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
        b2g.load_workspace('bar', StringIO(BAR_B2G_JSON))
        b2g.load_workspace('baz', StringIO(BAZ_B2G_JSON))
        labels = b2g.get_initial_targets('foo')
        rule = list(b2g.load_rules(labels))[0]
        gn_target = b2g.convert_rule(rule)
        self.assertEqual(gn_target.attrs['cflags'], ['-cflag'])
        self.assertEqual(gn_target.attrs['defines'], ['LOCAL_DEFINE'])
        self.assertEqual(
            gn_target.attrs['deps'],
            ['$dir_pw_third_party/baz/baz/pkg:baz_target'],
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
                '$dir_pw_third_party/bar/bar/pkg:bar_target1',
                '$dir_pw_third_party/bar/bar/pkg:bar_target2',
            ],
        )
        self.assertEqual(
            gn_target.attrs['sources'], ['$dir_pw_third_party_foo/src/foo.cc']
        )

    @mock.patch('subprocess.run')
    def test_update_pw_package(self, mock_run):
        """Tests updating the pw_package file."""
        mock_run.side_effect = [
            mock.MagicMock(**retval)
            for retval in [
                {'stdout.decode.return_value': ''},  # foo: git fetch
                {'stdout.decode.return_value': 'some-tag'},
                {'stdout.decode.return_value': '2024-01-01 00:00:00'},
                {'stdout.decode.return_value': '2024-01-01 00:00:01'},
            ]
        ]
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
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

    @mock.patch('subprocess.run')
    def test_get_imports(self, mock_run):
        """Tests getting the GNI files needed for a GN target."""
        mock_run.side_effect = [
            mock.MagicMock(**retval)
            for retval in [
                {'stdout.decode.return_value': ''},  # foo: git fetch
                {'stdout.decode.return_value': ''},  # bar: git fetch
                {'stdout.decode.return_value': ''},  # baz: git fetch
                {'stdout.decode.return_value': FOO_RULE_JSON},
            ]
        ]
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
        b2g.load_workspace('bar', StringIO(BAR_B2G_JSON))
        b2g.load_workspace('baz', StringIO(BAZ_B2G_JSON))
        labels = b2g.get_initial_targets('foo')
        rule = list(b2g.load_rules(labels))[0]
        gn_target = b2g.convert_rule(rule)
        imports = set(b2g.get_imports(gn_target))
        self.assertEqual(imports, {'$dir_pw_third_party/foo/foo.gni'})

    @mock.patch('subprocess.run')
    def test_update_doc_rst(self, mock_run):
        """Tests updating the git revision in the docs."""
        mock_run.side_effect = [
            mock.MagicMock(**retval)
            for retval in [
                {'stdout.decode.return_value': ''},  # foo: git fetch
                {'stdout.decode.return_value': 'http://src/foo.git'},
                {'stdout.decode.return_value': 'deadbeeffeedface'},
            ]
        ]
        b2g = BazelToGnConverter(PW_ROOT)
        b2g.parse_args_gn(StringIO(ARGS_GN))
        b2g.load_workspace('foo', StringIO(FOO_B2G_JSON))
        inputs = (
            [f'preserved {i}' for i in range(10)]
            + ['.. DO NOT EDIT BELOW THIS LINE. Generated section.']
            + [f'overwritten {i}' for i in range(10)]
        )
        outputs = list(b2g.update_doc_rst('foo', inputs))
        self.assertEqual(len(outputs), 18)
        self.assertEqual(outputs[:11], inputs[:11])
        self.assertEqual(outputs[11], '')
        self.assertEqual(outputs[12], 'Version')
        self.assertEqual(outputs[13], '=======')
        self.assertEqual(
            outputs[14],
            'The update script was last run for revision `deadbeef`_.',
        )
        self.assertEqual(outputs[15], '')
        self.assertEqual(
            outputs[16],
            '.. _deadbeef: http://src/foo/tree/deadbeeffeedface',
        )
        self.assertEqual(outputs[17], '')


if __name__ == '__main__':
    unittest.main()
