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
"""Tests for the workflows launcher CLI."""

import json
import unittest

from google.protobuf import json_format
from pw_build.proto import workflows_pb2
from pw_build.workflows.launcher import WorkflowsCli


_EXAMPLE_CONFIG = """
{
  "configs": [
    {
      "name": "bazel_default",
      "description": "Default bazel build configuration (host)",
      "build_type": "bazel",
      "args": [],
      "env": {}
    }
  ],
  "builds": [
    {
      "name": "all_host",
      "use_config": "bazel_default",
      "targets": [
        "//..."
      ]
    },
    {
      "name": "all_host_cpp20",
      "build_config": {
        "name": "bazel_default_cpp20",
        "description": "Host C++20 build",
        "build_type": "bazel",
        "args": [
          "--//pw_toolchain/cc:cxx_standard=20"
        ]
      },
      "targets": [
        "//..."
      ]
    },
    {
      "name": "docs",
      "use_config": "bazel_default",
      "targets": [
        "//docs"
      ]
    },
    {
      "name": "build_rp2040_tests",
      "build_config":{
        "name": "pico_rp2040",
        "description": "Default Pi Pico build (rp2040)",
        "build_type": "bazel",
        "args": [
          "--config=rp2040",
          "--build_tests_only"
        ]
      },
      "targets": [
        "//..."
      ]
    },
    {
      "name": "build_rp2350_tests",
      "build_config": {
        "name": "pico_rp2350",
        "description": "Default Pi Pico build (rp2350)",
        "build_type": "bazel",
        "args": [
          "--config=rp2350",
          "--build_tests_only"
        ]
      },
      "targets": [
        "//..."
      ]
    }
  ],
  "tools": [
    {
      "name": "format",
      "description": "Find and fix code formatting issues",
      "use_config": "bazel_default",
      "analyzer_friendly_args": ["--check"],
      "target": "@pigweed//:format"
    },
    {
      "name": "owners_lint",
      "description": "Identify OWNERS file issues",
      "use_config": "bazel_default",
      "target": "@pigweed//pw_presubmit/py:owners_lint",
      "type": "ANALYZER"
    }
  ],
  "groups": [
    {
      "name": "presubmit",
      "builds": [
        "all_host",
        "docs",
        "rp2040",
        "rp2350"
      ],
      "analyzers": [
        "format",
        "owners_lint"
      ]
    }
  ]
}
"""


class TestWorkflowsLauncher(unittest.TestCase):
    """Tests for the workflows launcher."""

    @staticmethod
    def _load_config(json_config: str):
        json_msg = json.loads(json_config)
        msg = workflows_pb2.WorkflowSuite()
        json_format.ParseDict(json_msg, msg)
        return WorkflowsCli(msg)

    def test_tool(self):
        cli = self._load_config(_EXAMPLE_CONFIG)
        expected = '\n'.join(
            (
                'name: "format"',
                'description: "Find and fix code formatting issues"',
                'use_config: "bazel_default"',
                'target: "@pigweed//:format"',
                'analyzer_friendly_args: "--check"',
            )
        )
        actual = cli.dump_fragment('format').strip()
        self.assertEqual(actual, expected)

    def test_group(self):
        cli = self._load_config(_EXAMPLE_CONFIG)
        expected = '\n'.join(
            (
                'name: "presubmit"',
                'builds: "all_host"',
                'builds: "docs"',
                'builds: "rp2040"',
                'builds: "rp2350"',
                'analyzers: "format"',
                'analyzers: "owners_lint"',
            )
        )
        actual = cli.dump_fragment('presubmit').strip()
        self.assertEqual(actual, expected)

    def test_build(self):
        cli = self._load_config(_EXAMPLE_CONFIG)
        expected = '\n'.join(
            (
                'name: "build_rp2040_tests"',
                'build_config {',
                '  name: "pico_rp2040"',
                '  description: "Default Pi Pico build (rp2040)"',
                '  build_type: "bazel"',
                '  args: "--config=rp2040"',
                '  args: "--build_tests_only"',
                '}',
                'targets: "//..."',
            )
        )
        actual = cli.dump_fragment('build_rp2040_tests').strip()
        self.assertEqual(actual, expected)

    def test_build_config(self):
        cli = self._load_config(_EXAMPLE_CONFIG)
        expected = '\n'.join(
            (
                'name: "bazel_default"',
                'description: "Default bazel build configuration (host)"',
                'build_type: "bazel"',
            )
        )
        actual = cli.dump_fragment('bazel_default').strip()
        self.assertEqual(actual, expected)

    def test_empty_config(self):
        cli = self._load_config('{}')
        with self.assertRaises(ValueError):
            cli.dump_fragment('bazel_default').strip()

    def test_empty_fragment_name(self):
        cli = self._load_config(_EXAMPLE_CONFIG)
        with self.assertRaises(ValueError):
            cli.dump_fragment('').strip()


if __name__ == '__main__':
    unittest.main()
