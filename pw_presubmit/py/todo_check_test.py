#!/usr/bin/env python3
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
"""Tests for todo_check."""

from pathlib import Path
import re
import unittest
from unittest.mock import MagicMock, mock_open, patch

from pw_presubmit import todo_check

# pylint: disable=attribute-defined-outside-init
# todo-check: disable


# pylint: disable-next=too-many-public-methods
class TestTodoCheck(unittest.TestCase):
    """Test TODO checker."""

    def _run(self, regex: re.Pattern, contents: str) -> None:
        self.ctx = MagicMock()
        self.ctx.fail = MagicMock()
        path = MagicMock(spec=Path('foo/bar'))

        def mocked_open_read(*args, **kwargs):
            return mock_open(read_data=contents)(*args, **kwargs)

        with patch.object(path, 'open', mocked_open_read):
            # pylint: disable=protected-access
            todo_check._process_file(self.ctx, regex, path)

            # pylint: enable=protected-access

    def _run_bugs_users(self, contents: str) -> None:
        self._run(todo_check.BUGS_OR_USERNAMES, contents)

    def _run_bugs(self, contents: str) -> None:
        self._run(todo_check.BUGS_ONLY, contents)

    def test_one_bug_legacy(self) -> None:
        contents = 'TODO(b/123): foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_one_bug_new(self) -> None:
        contents = 'TODO: b/123 - foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_one_bug_full_url(self) -> None:
        contents = 'TODO: https://issues.pigweed.dev/issues/123 - foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_two_bugs_legacy(self) -> None:
        contents = 'TODO(b/123, b/456): foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_two_bugs_new(self) -> None:
        contents = 'TODO: b/123, b/456 - foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_three_bugs_legacy(self) -> None:
        contents = 'TODO(b/123,b/456,b/789): foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_three_bugs_new(self) -> None:
        contents = 'TODO: b/123,b/456,b/789 - foo\n'
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_one_username_legacy(self) -> None:
        self._run_bugs_users('TODO(usera): foo\n')
        self.ctx.fail.assert_not_called()

    def test_one_username_new(self) -> None:
        self._run_bugs_users('TODO: usera@ - foo\n')
        self.ctx.fail.assert_not_called()

    def test_one_username_new_noat(self) -> None:
        self._run_bugs_users('TODO: usera - foo\n')
        self.ctx.fail.assert_called()

    def test_one_username_new_short_domain(self) -> None:
        self._run_bugs_users('TODO: usera@com - foo\n')
        self.ctx.fail.assert_called()

    def test_one_username_new_medium_domain(self) -> None:
        self._run_bugs_users('TODO: usera@example.com - foo\n')
        self.ctx.fail.assert_not_called()

    def test_one_username_new_long_domain(self) -> None:
        self._run_bugs_users('TODO: usera@a.b.c.d.example.com - foo\n')
        self.ctx.fail.assert_not_called()

    def test_two_usernames_legacy(self) -> None:
        self._run_bugs_users('TODO(usera, userb): foo\n')
        self.ctx.fail.assert_not_called()

    def test_two_usernames_new(self) -> None:
        self._run_bugs_users('TODO: usera@, userb@ - foo\n')
        self.ctx.fail.assert_not_called()

    def test_three_usernames_legacy(self) -> None:
        self._run_bugs_users('TODO(usera,userb,userc): foo\n')
        self.ctx.fail.assert_not_called()

    def test_three_usernames_new(self) -> None:
        self._run_bugs_users('TODO: usera@,userb@example.com,userc@ - foo\n')
        self.ctx.fail.assert_not_called()

    def test_username_not_allowed_legacy(self) -> None:
        self._run_bugs('TODO(usera): foo\n')
        self.ctx.fail.assert_called()

    def test_username_not_allowed_new(self) -> None:
        self._run_bugs('TODO: usera@ - foo\n')
        self.ctx.fail.assert_called()

    def test_space_after_todo_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO (b/123): foo\n')
        self.ctx.fail.assert_called()

    def test_space_after_todo_bugsonly_new(self) -> None:
        self._run_bugs('TODO : b/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_space_after_todo_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO (b/123): foo\n')
        self.ctx.fail.assert_called()

    def test_space_after_todo_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO : b/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_space_before_bug_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO( b/123): foo\n')
        self.ctx.fail.assert_called()

    def test_no_space_before_bug_bugsonly_new(self) -> None:
        self._run_bugs('TODO:b/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_space_before_bug_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO( b/123): foo\n')
        self.ctx.fail.assert_called()

    def test_no_space_before_bug_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO:b/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_space_after_bug_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO(b/123 ): foo\n')
        self.ctx.fail.assert_called()

    def test_no_space_after_bug_bugsonly_new(self) -> None:
        self._run_bugs('TODO: b/123- foo\n')
        self.ctx.fail.assert_called()

    def test_no_space_before_explanation_bugsonly_new(self) -> None:
        self._run_bugs('TODO: b/123 -foo\n')
        self.ctx.fail.assert_called()

    def test_space_after_bug_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO(b/123 ): foo\n')
        self.ctx.fail.assert_called()

    def test_no_space_before_explanation_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO: b/123 -foo\n')
        self.ctx.fail.assert_called()

    def test_missing_explanation_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO: b/123 -\n')
        self.ctx.fail.assert_called()

    def test_missing_explanation_bugsonly_new(self) -> None:
        self._run_bugs('TODO: b/123\n')
        self.ctx.fail.assert_called()

    def test_missing_explanation_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO: b/123 -\n')
        self.ctx.fail.assert_called()

    def test_missing_explanation_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO: b/123\n')
        self.ctx.fail.assert_called()

    def test_not_a_bug_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO(cl/123): foo\n')
        self.ctx.fail.assert_called()

    def test_not_a_bug_bugsonly_new(self) -> None:
        self._run_bugs('TODO: cl/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_not_a_bug_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO(cl/123): foo\n')
        self.ctx.fail.assert_called()

    def test_not_a_bug_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO: cl/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_but_not_bug_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO(b/123, cl/123): foo\n')
        self.ctx.fail.assert_called()

    def test_but_not_bug_bugsonly_new(self) -> None:
        self._run_bugs('TODO: b/123, cl/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_bug_not_bug_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO(b/123, cl/123): foo\n')
        self.ctx.fail.assert_called()

    def test_bug_not_bug_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO: b/123, cl/123 - foo\n')
        self.ctx.fail.assert_called()

    def test_empty_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO(): foo\n')
        self.ctx.fail.assert_called()

    def test_empty_bugsonly_new(self) -> None:
        self._run_bugs('TODO: - foo\n')
        self.ctx.fail.assert_called()

    def test_empty_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO(): foo\n')
        self.ctx.fail.assert_called()

    def test_empty_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO: - foo\n')
        self.ctx.fail.assert_called()

    def test_bare_bugsonly_legacy(self) -> None:
        self._run_bugs('TODO: foo\n')
        self.ctx.fail.assert_called()

    def test_bare_bugsonly_new(self) -> None:
        self._run_bugs('TODO: foo\n')
        self.ctx.fail.assert_called()

    def test_bare_bugsusers_legacy(self) -> None:
        self._run_bugs_users('TODO: foo\n')
        self.ctx.fail.assert_called()

    def test_bare_bugsusers_new(self) -> None:
        self._run_bugs_users('TODO: foo\n')
        self.ctx.fail.assert_called()

    def test_fuchsia(self) -> None:
        self._run_bugs_users('TODO(fxbug.dev/123): foo\n')
        self.ctx.fail.assert_not_called()

    def test_fuchsia_two_bugs(self) -> None:
        self._run_bugs_users('TODO(fxbug.dev/123,fxbug.dev/321): bar\n')
        self.ctx.fail.assert_not_called()

    def test_bazel_gh_issue(self) -> None:
        contents = (
            'TODO: https://github.com/bazelbuild/bazel/issues/12345 - '
            'Bazel sometimes works\n'
        )
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()

    def test_bazel_gh_issue_underscore(self) -> None:
        contents = (
            'TODO: https://github.com/bazelbuild/rules_cc/issues/678910 - '
            'Sometimes it does not work\n'
        )
        self._run_bugs_users(contents)
        self.ctx.fail.assert_not_called()
        self._run_bugs(contents)
        self.ctx.fail.assert_not_called()


if __name__ == '__main__':
    unittest.main()

# todo-check: enable
