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
"""Tests for inclusive_language."""

from pathlib import Path
import tempfile
import unittest

from pw_presubmit import inclusive_language

# pylint: disable=attribute-defined-outside-init
# pylint: disable=too-many-public-methods

IGNORE = inclusive_language.IGNORE
DISABLE = inclusive_language.DISABLE
ENABLE = inclusive_language.ENABLE

# inclusive-language: disable


class TestInclusiveLanguage(unittest.TestCase):
    """Test inclusive language check."""

    def _run(self, *contents: str, filename: str | None = None) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            path = Path(tempdir) / (filename or 'foo')

            with path.open('w') as outs:
                outs.write('\n'.join(contents))

            self.found_words: dict[
                Path,
                list[
                    inclusive_language.PathMatch | inclusive_language.LineMatch
                ],
            ] = {}

            inclusive_language.check_file(
                path,
                self.found_words,
                check_path=bool(filename),
            )
            self.success = True

    def assert_success(self):
        self.assertFalse(self.found_words)

    def assert_failures(self, num_expected_failures):
        num_actual_failures = sum(len(x) for x in self.found_words.values())
        self.assertEqual(num_expected_failures, num_actual_failures)

    def test_no_objectionable_language(self) -> None:
        self._run('no objectionable language')
        self.assert_success()

    def test_slave(self) -> None:
        self._run('slave')
        self.assert_failures(1)

    def test_multiline_slave(self) -> None:
        self._run('prefix', 'slave', 'suffix')
        self.assert_failures(1)

    def test_plural(self) -> None:
        self._run('slaves')
        self.assert_failures(1)

    def test_past_tense(self) -> None:
        self._run('slaved')
        self.assert_failures(1)

    def test_ignore_same_line(self) -> None:
        self._run(f'slave {IGNORE}')
        self.assert_success()

    def test_ignore_next_line(self) -> None:
        self._run(IGNORE, 'slave')
        self.assert_success()

    def test_ignore_same_and_next_line(self) -> None:
        self._run(f'slave {IGNORE}', 'slave')
        self.assert_success()

    def test_ignore_prev_line(self) -> None:
        self._run('slave', IGNORE)
        self.assert_failures(1)

    def test_one_ignored_one_not(self) -> None:
        self._run(IGNORE, 'slave', 'slave')
        self.assert_failures(1)

    def test_two_ignored_one_not(self) -> None:
        self._run(f'slave {IGNORE}', 'slave', 'slave')
        self.assert_failures(1)

    def test_disable_next_line(self) -> None:
        self._run(DISABLE, 'slave')
        self.assert_success()

    def test_disable_prev_line(self) -> None:
        self._run('slave', DISABLE)
        self.assert_failures(1)

    def test_disable_line_enable(self) -> None:
        self._run(DISABLE, 'slave', ENABLE)
        self.assert_success()

    def test_line_disable_enable(self) -> None:
        self._run('slave', DISABLE, ENABLE)
        self.assert_failures(1)

    def test_disable_enable_line(self) -> None:
        self._run(DISABLE, ENABLE, 'slave')
        self.assert_failures(1)

    def test_one_enabled_one_disabled_one_enabled(self) -> None:
        self._run('slave', DISABLE, 'slave', ENABLE, 'slave')
        self.assert_failures(2)

    def test_repeated_disable_noop(self) -> None:
        self._run(DISABLE, DISABLE, 'slave', ENABLE, 'slave')
        self.assert_failures(1)

    def test_repeated_enable_noop(self) -> None:
        self._run(DISABLE, 'slave', ENABLE, ENABLE, 'slave')
        self.assert_failures(1)

    def test_camel_case(self) -> None:
        self._run('FooSlaveBar')
        self.assert_failures(1)

    def test_underscores(self) -> None:
        self._run('foo_slave_bar')
        self.assert_failures(1)

    def test_init_camel_case(self) -> None:
        self._run('SlaveBar')
        self.assert_failures(1)

    def test_init_underscores(self) -> None:
        self._run('slave_bar')
        self.assert_failures(1)

    def test_underscore_camel_case(self) -> None:
        self._run('foo_SlaveBar')
        self.assert_failures(1)

    def test_camel_case_underscore(self) -> None:
        self._run('FooSlave_bar')
        self.assert_failures(1)

    def test_number_prefix(self) -> None:
        self._run('5slave')
        self.assert_failures(1)

    def test_number_suffix(self) -> None:
        self._run('slave5')
        self.assert_failures(1)

    def test_konstant(self) -> None:
        self._run('kSLAVE')
        self.assert_failures(1)

    def test_command_line_argument(self) -> None:
        self._run('--master-disable')
        self.assert_failures(1)

    def test_multiple(self) -> None:
        self._run('master', 'slave')
        self.assert_failures(2)

    def test_bad_filename(self) -> None:
        self._run(filename='slave')
        self.assert_failures(1)


if __name__ == '__main__':
    unittest.main()

# inclusive-language: enable
