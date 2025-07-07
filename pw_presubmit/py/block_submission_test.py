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
"""Tests for block_submission."""

from pathlib import Path
import tempfile
import unittest

from pw_presubmit import inclusive_language, block_submission

# block-submission: disable


class TestBlockSubmission(unittest.TestCase):
    """Test the submission-blocking phrase check.

    Tests the contraction instead of the full phrase to avoid issues with other
    similar tools.
    """

    def setUp(self) -> None:
        self.found_words: dict[
            Path,
            list[inclusive_language.PathMatch | inclusive_language.LineMatch],
        ] = {}

    def _run(self, *contents: str, filename: str | None = None) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            path = Path(tempdir) / (filename or 'contents')
            path.write_text('\n'.join(contents))

            inclusive_language.check_file(
                path,
                self.found_words,
                words_regex=block_submission.WORDS_REGEX,
                disable_tag=block_submission.DISABLE_TAG,
                enable_tag=block_submission.ENABLE_TAG,
                ignore_tag=block_submission.IGNORE_TAG,
                issue_type=block_submission.ISSUE_TYPE,
                check_path=bool(filename),
            )

    def assert_success(self):
        self.assertFalse(self.found_words)

    def assert_failures(self, num_expected_failures):
        num_actual_failures = sum(len(x) for x in self.found_words.values())
        self.assertEqual(num_expected_failures, num_actual_failures)

    def test_no_blockers(self) -> None:
        self._run('no blockers')
        self.assert_success()

    def test_dont_submit(self) -> None:
        self._run("don't submit")
        self.assert_failures(1)

    def test_dont_submit_no_apostrophe(self) -> None:
        self._run('dont submit')
        self.assert_failures(1)

    def test_dont_submit_hyphen(self) -> None:
        self._run('dont-submit')
        self.assert_failures(1)

    def test_dont_submit_underscore(self) -> None:
        self._run('dont_submit')
        self.assert_failures(1)

    def test_dont_submit_no_space(self) -> None:
        self._run("dontsubmit")
        self.assert_failures(1)

    def test_dont_submit_multiple_characters(self) -> None:
        self._run("dont - _ submit")
        self.assert_failures(1)

    def test_multiline_blocker(self) -> None:
        self._run('prefix', 'dont submit', 'suffix')
        self.assert_failures(1)

    def test_ignore_same_line(self) -> None:
        self._run(f'dont submit {block_submission.IGNORE_TAG}')
        self.assert_success()

    def test_ignore_next_line(self) -> None:
        self._run(block_submission.IGNORE_TAG, 'dont submit')
        self.assert_success()

    def test_disable_enable(self) -> None:
        self._run(
            'dont submit',
            block_submission.DISABLE_TAG,
            'dont submit',
            block_submission.ENABLE_TAG,
            'dont submit',
        )
        self.assert_failures(2)

    def test_bad_filename(self) -> None:
        self._run(filename='dont_submit.txt')
        self.assert_failures(1)


# block-submission: enable

if __name__ == '__main__':
    unittest.main()
