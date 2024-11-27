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
"""Unit tests for bazel_checks.py."""

import os
import pathlib
import shutil
import tempfile
import unittest
from unittest import mock

from pw_presubmit import presubmit, build, bazel_checks


def _write_two_targets_to_stdout(*args, stdout=None, **kwargs):
    del args, kwargs  # Unused.
    if stdout is None:
        return

    stdout.write("//one:target\n//other:target\n")


def _write_nothing_to_stdout(*args, stdout=None, **kwargs):
    del args, kwargs  # Unused.
    if stdout is None:
        return
    stdout.close()


class IncludePresubmitCheckTest(unittest.TestCase):
    """Tests of the include_presubmit_check."""

    def setUp(self):
        fd, self.failure_summary_log = tempfile.mkstemp()
        os.close(fd)
        self.output_dir = tempfile.mkdtemp()

        self.ctx = mock.MagicMock()
        self.ctx.failed = False
        self.ctx.failure_summary_log = pathlib.Path(self.failure_summary_log)
        self.ctx.output_dir = pathlib.Path(self.output_dir)

    def tearDown(self):
        os.remove(self.failure_summary_log)
        shutil.rmtree(self.output_dir)

    @mock.patch.object(
        build, 'bazel', autospec=True, side_effect=_write_two_targets_to_stdout
    )
    def test_query_returns_two_targets(self, _):
        check = bazel_checks.includes_presubmit_check('//...')
        self.assertEqual(check(self.ctx), presubmit.PresubmitResult.FAIL)
        self.assertIn('//one:target', self.ctx.failure_summary_log.read_text())
        self.assertIn(
            '//other:target', self.ctx.failure_summary_log.read_text()
        )

    @mock.patch.object(
        build, 'bazel', autospec=True, side_effect=_write_nothing_to_stdout
    )
    def test_query_returns_nothing(self, _):
        check = bazel_checks.includes_presubmit_check('//...')
        self.assertEqual(check(self.ctx), presubmit.PresubmitResult.PASS)
        self.assertNotIn(
            '//one:target', self.ctx.failure_summary_log.read_text()
        )
        self.assertNotIn(
            '//other:target', self.ctx.failure_summary_log.read_text()
        )


if __name__ == "__main__":
    unittest.main()
