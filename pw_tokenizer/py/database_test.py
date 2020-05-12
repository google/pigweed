#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Tests for the database module."""

import io
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from pw_tokenizer import database

ELF = Path(__file__).parent / 'example_binary_with_tokenized_strings.elf'

CSV_DEFAULT_DOMAIN = '''\
00000000,          ,""
141c35d5,          ,"The answer: ""%s"""
2b78825f,          ,"[:-)"
2e668cd6,          ,"Jello, world!"
31631781,          ,"%d"
61fd1e26,          ,"%ld"
68ab92da,          ,"%s there are %x (%.2f) of them%c"
7b940e2a,          ,"Hello %s! %hd %e"
7da55d52,          ,">:-[]"
851beeb6,          ,"%u %d"
881436a0,          ,"The answer is: %s"
88808930,          ,"%u%d%02x%X%hu%hhd%d%ld%lu%lld%llu%c%c%c"
ad002c97,          ,"%llx"
b3653e13,          ,"Jello!"
cc6d3131,          ,"Jello?"
e13b0f94,          ,"%llu"
e65aefef,          ,"Won't fit : %s%d"
'''

CSV_TEST_DOMAIN = '''\
00000000,          ,""
59b2701c,          ,"The answer was: %s"
881436a0,          ,"The answer is: %s"
'''

CSV_ALL_DOMAINS = '''\
00000000,          ,""
141c35d5,          ,"The answer: ""%s"""
2b78825f,          ,"[:-)"
2e668cd6,          ,"Jello, world!"
31631781,          ,"%d"
59b2701c,          ,"The answer was: %s"
61fd1e26,          ,"%ld"
68ab92da,          ,"%s there are %x (%.2f) of them%c"
7b940e2a,          ,"Hello %s! %hd %e"
7da55d52,          ,">:-[]"
851beeb6,          ,"%u %d"
881436a0,          ,"The answer is: %s"
88808930,          ,"%u%d%02x%X%hu%hhd%d%ld%lu%lld%llu%c%c%c"
ad002c97,          ,"%llx"
b3653e13,          ,"Jello!"
cc6d3131,          ,"Jello?"
e13b0f94,          ,"%llu"
e65aefef,          ,"Won't fit : %s%d"
'''


def run_cli(*args):
    original_argv = sys.argv
    sys.argv = ['database.py', *(str(a) for a in args)]
    # pylint: disable=protected-access
    try:
        database._main(*database._parse_args())
    finally:
        # Remove the log handler added by _main to avoid duplicate logs.
        if database._LOG.handlers:
            database._LOG.handlers.pop()
        # pylint: enable=protected-access

        sys.argv = original_argv


def _mock_output():
    output = io.BytesIO()
    output.name = '<fake stdout>'
    return io.TextIOWrapper(output, write_through=True)


REPORT_DEFAULT_DOMAIN = b'''\
example_binary_with_tokenized_strings.elf]
                 Domain: default
        Entries present: 17
        Size of strings: 205 B
          Total entries: 17
  Total size of strings: 205 B
             Collisions: 0 tokens
'''

REPORT_TEST_DOMAIN = b'''\
example_binary_with_tokenized_strings.elf]
                 Domain: TEST_DOMAIN
        Entries present: 3
        Size of strings: 38 B
          Total entries: 3
  Total size of strings: 38 B
             Collisions: 0 tokens
'''


class DatabaseCommandLineTest(unittest.TestCase):
    """Tests the database.py command line interface."""
    def setUp(self):
        self._dir = Path(tempfile.mkdtemp('_pw_tokenizer_test'))
        self._csv = self._dir / 'db.csv'

    def tearDown(self):
        shutil.rmtree(self._dir)

    def test_create_csv(self):
        run_cli('create', '--database', self._csv, ELF)

        self.assertEqual(CSV_DEFAULT_DOMAIN, self._csv.read_text())

    def test_create_csv_test_domain(self):
        run_cli('create', '--database', self._csv, f'{ELF}#TEST_DOMAIN')

        self.assertEqual(CSV_TEST_DOMAIN, self._csv.read_text())

    def test_create_csv_all_domains(self):
        run_cli('create', '--database', self._csv, f'{ELF}#.*')

        self.assertEqual(CSV_ALL_DOMAINS, self._csv.read_text())

    def test_create_force(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        with self.assertRaises(FileExistsError):
            run_cli('create', '--database', self._csv, ELF)

        run_cli('create', '--force', '--database', self._csv, ELF)

    def test_create_binary(self):
        binary = self._dir / 'db.bin'
        run_cli('create', '--type', 'binary', '--database', binary, ELF)

        # Write the binary database as CSV to verify its contents.
        run_cli('create', '--database', self._csv, binary)

        self.assertEqual(CSV_DEFAULT_DOMAIN, self._csv.read_text())

    def test_add(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        run_cli('add', '--database', self._csv, f'{ELF}#TEST_DOMAIN')
        self.assertEqual(CSV_ALL_DOMAINS, self._csv.read_text())

    def test_mark_removals(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        run_cli('mark_removals', '--database', self._csv, '--date',
                '1998-09-04', f'{ELF}#default')

        # Add the removal date to the token not in the default domain
        new_csv = CSV_ALL_DOMAINS.replace('59b2701c,          ,',
                                          '59b2701c,1998-09-04,')
        self.assertNotEqual(CSV_ALL_DOMAINS, new_csv)

        self.assertEqual(new_csv, self._csv.read_text())

    def test_purge(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        # Mark everything not in TEST_DOMAIN as removed.
        run_cli('mark_removals', '--database', self._csv, f'{ELF}#TEST_DOMAIN')

        # Delete all entries except those in TEST_DOMAIN.
        run_cli('purge', '--database', self._csv)

        self.assertEqual(CSV_TEST_DOMAIN, self._csv.read_text())

    @mock.patch('sys.stdout', new_callable=_mock_output)
    def test_report(self, mock_stdout):
        run_cli('report', ELF)
        self.assertIn(REPORT_DEFAULT_DOMAIN, mock_stdout.buffer.getvalue())
        self.assertIn(REPORT_TEST_DOMAIN, mock_stdout.buffer.getvalue())


if __name__ == '__main__':
    unittest.main()
