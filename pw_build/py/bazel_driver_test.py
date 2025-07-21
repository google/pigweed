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
"""Tests for the bazel_driver."""

import unittest

from pw_build.workflows.bazel_driver import BazelBuildDriver
from pw_build.proto import build_driver_pb2

_BAZEL_BUILD_JOB_REQUEST = """
{
  "jobs": [
    {
      "build": {
        "build_config": {
          "build_type": "bazel",
          "args": ["--config=rp2040"],
          "env": {
            "FOO": "bar"
          }
        },
        "targets": [
          "//..."
        ]
      }
    }
  ]
}
"""

_BAZEL_BUILD_NO_TEST_JOB_REQUEST = """
{
  "jobs": [
    {
      "build": {
        "build_config": {
          "build_type": "bazel",
          "args": ["--config=rp2040"],
          "driver_options": {
            "@type": "pw.build.proto.BazelDriverOptions",
            "no_test": true
          }
        },
        "targets": [
          "//..."
        ]
      }
    }
  ]
}
"""

_BAZEL_TOOL_JOB_REQUEST = """
{
  "jobs": [
    {
      "tool": {
        "build_config": {
          "build_type": "bazel",
          "args": ["--subcommands"],
          "env": {
            "BAZ": "qux"
          }
        },
        "target": "@pigweed//:format"
      }
    }
  ]
}
"""


class TestBazelBuildDriver(unittest.TestCase):
    """Tests for the bazel_driver."""

    def test_bazel_build_driver_for_build(self):
        """Checks that all properties are handled during a build sequence."""
        driver = BazelBuildDriver()
        response = driver.generate_jobs_from_json(_BAZEL_BUILD_JOB_REQUEST)
        self.assertEqual(len(response.jobs), 1)

        # Verify the build job
        build_job = response.jobs[0]
        self.assertEqual(len(build_job.actions), 3)

        # Check canonicalize-flags action for build
        self.assertEqual(build_job.actions[0].executable, 'bazelisk')
        self.assertEqual(
            list(build_job.actions[0].args),
            ['canonicalize-flags', '--config=rp2040'],
        )
        self.assertEqual(build_job.actions[0].env['FOO'], 'bar')

        # Check build action
        self.assertEqual(build_job.actions[1].executable, 'bazelisk')
        self.assertEqual(
            list(build_job.actions[1].args),
            ['build', '--config=rp2040', '//...'],
        )
        self.assertEqual(build_job.actions[1].env['FOO'], 'bar')

        # Check test action
        self.assertEqual(build_job.actions[2].executable, 'bazelisk')
        self.assertEqual(
            list(build_job.actions[2].args),
            ['test', '--config=rp2040', '//...'],
        )
        self.assertEqual(build_job.actions[2].env['FOO'], 'bar')

        # Ensure all Bazel commands are run from the invoker's CWD.
        for action in build_job.actions:
            self.assertEqual(
                action.run_from,
                build_driver_pb2.Action.InvocationLocation.INVOKER_CWD,
            )

    def test_bazel_build_driver_for_build_with_no_test(self):
        """Checks that all properties are handled during a build sequence."""
        driver = BazelBuildDriver()
        response = driver.generate_jobs_from_json(
            _BAZEL_BUILD_NO_TEST_JOB_REQUEST
        )
        self.assertEqual(len(response.jobs), 1)

        # Verify the build job
        build_job = response.jobs[0]
        self.assertEqual(len(build_job.actions), 2)

        # Check canonicalize-flags action for build
        self.assertEqual(build_job.actions[0].executable, 'bazelisk')
        self.assertEqual(
            list(build_job.actions[0].args),
            ['canonicalize-flags', '--config=rp2040'],
        )

        # Check build action
        self.assertEqual(build_job.actions[1].executable, 'bazelisk')
        self.assertEqual(
            list(build_job.actions[1].args),
            ['build', '--config=rp2040', '//...'],
        )

        # Ensure all Bazel commands are run from the invoker's CWD.
        for action in build_job.actions:
            self.assertEqual(
                action.run_from,
                build_driver_pb2.Action.InvocationLocation.INVOKER_CWD,
            )

    def test_bazel_build_driver_for_tool(self):
        """Checks that all properties are handled during a tool sequence."""
        driver = BazelBuildDriver()
        response = driver.generate_jobs_from_json(_BAZEL_TOOL_JOB_REQUEST)
        self.assertEqual(len(response.jobs), 1)

        # Verify the tool job
        tool_job = response.jobs[0]
        self.assertEqual(len(tool_job.actions), 2)

        # Check canonicalize-flags action for tool
        self.assertEqual(tool_job.actions[0].executable, 'bazelisk')
        self.assertEqual(
            list(tool_job.actions[0].args),
            ['canonicalize-flags', '--subcommands'],
        )
        self.assertEqual(tool_job.actions[0].env['BAZ'], 'qux')

        # Check run action
        self.assertEqual(tool_job.actions[1].executable, 'bazelisk')
        self.assertEqual(
            list(tool_job.actions[1].args),
            [
                'run',
                '--ui_event_filters=FATAL,ERROR,PROGRESS',
                '--subcommands',
                '@pigweed//:format',
                '--',
                '${FORWARDED_LAUNCH_ARGS}',
            ],
        )
        self.assertEqual(tool_job.actions[1].env['BAZ'], 'qux')

        # Ensure all Bazel commands are run from the invoker's CWD.
        for action in tool_job.actions:
            self.assertEqual(
                action.run_from,
                build_driver_pb2.Action.InvocationLocation.INVOKER_CWD,
            )

    def test_nop(self):
        driver = BazelBuildDriver()
        response = driver.generate_jobs_from_json('{}')
        self.assertEqual(len(response.jobs), 0)

    def test_empty_job(self):
        driver = BazelBuildDriver()
        response = driver.generate_jobs_from_json('{"jobs":[{}]}')
        self.assertEqual(len(response.jobs), 1)
        job = response.jobs[0]
        self.assertEqual(len(job.actions), 0)


if __name__ == '__main__':
    unittest.main()
