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

import json
import io
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from pw_tokenizer import database

# This is an ELF file with only the pw_tokenizer sections. It was created
# from a tokenize_test binary built for the STM32F429i Discovery board. The
# pw_tokenizer sections were extracted with this command:
#
#   arm-none-eabi-objcopy -S --only-section ".pw_tokenize*" <ELF> <OUTPUT>
#
TOKENIZED_ENTRIES_ELF = Path(
    __file__).parent / 'example_binary_with_tokenized_strings.elf'
LEGACY_PLAIN_STRING_ELF = Path(
    __file__).parent / 'example_legacy_binary_with_tokenized_strings.elf'

CSV_DEFAULT_DOMAIN = '''\
00000000,          ,""
141c35d5,          ,"The answer: ""%s"""
29aef586,          ,"1234"
2b78825f,          ,"[:-)"
2e668cd6,          ,"Jello, world!"
31631781,          ,"%d"
61fd1e26,          ,"%ld"
68ab92da,          ,"%s there are %x (%.2f) of them%c"
7b940e2a,          ,"Hello %s! %hd %e"
7da55d52,          ,">:-[]"
7f35a9a5,          ,"TestName"
851beeb6,          ,"%u %d"
881436a0,          ,"The answer is: %s"
88808930,          ,"%u%d%02x%X%hu%hhd%d%ld%lu%lld%llu%c%c%c"
92723f44,          ,"???"
a09d6698,          ,"won-won-won-wonderful"
aa9ffa66,          ,"void pw::tokenizer::{anonymous}::TestName()"
ad002c97,          ,"%llx"
b3653e13,          ,"Jello!"
cc6d3131,          ,"Jello?"
e13b0f94,          ,"%llu"
e65aefef,          ,"Won't fit : %s%d"
'''

CSV_TEST_DOMAIN = """\
17fa86d3,          ,"hello"
18c5017c,          ,"yes"
59b2701c,          ,"The answer was: %s"
881436a0,          ,"The answer is: %s"
d18ada0f,          ,"something"
"""

CSV_ALL_DOMAINS = '''\
00000000,          ,""
141c35d5,          ,"The answer: ""%s"""
17fa86d3,          ,"hello"
18c5017c,          ,"yes"
29aef586,          ,"1234"
2b78825f,          ,"[:-)"
2e668cd6,          ,"Jello, world!"
31631781,          ,"%d"
59b2701c,          ,"The answer was: %s"
61fd1e26,          ,"%ld"
68ab92da,          ,"%s there are %x (%.2f) of them%c"
7b940e2a,          ,"Hello %s! %hd %e"
7da55d52,          ,">:-[]"
7f35a9a5,          ,"TestName"
851beeb6,          ,"%u %d"
881436a0,          ,"The answer is: %s"
88808930,          ,"%u%d%02x%X%hu%hhd%d%ld%lu%lld%llu%c%c%c"
92723f44,          ,"???"
a09d6698,          ,"won-won-won-wonderful"
aa9ffa66,          ,"void pw::tokenizer::{anonymous}::TestName()"
ad002c97,          ,"%llx"
b3653e13,          ,"Jello!"
cc6d3131,          ,"Jello?"
d18ada0f,          ,"something"
e13b0f94,          ,"%llu"
e65aefef,          ,"Won't fit : %s%d"
'''

EXPECTED_REPORT = {
    str(TOKENIZED_ENTRIES_ELF): {
        '': {
            'present_entries': 22,
            'present_size_bytes': 289,
            'total_entries': 22,
            'total_size_bytes': 289,
            'collisions': 0
        },
        'TEST_DOMAIN': {
            'present_entries': 5,
            'present_size_bytes': 57,
            'total_entries': 5,
            'total_size_bytes': 57,
            'collisions': 0
        }
    }
}


def run_cli(*args) -> None:
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


def _mock_output() -> io.TextIOWrapper:
    output = io.BytesIO()
    output.name = '<fake stdout>'
    return io.TextIOWrapper(output, write_through=True)


class DatabaseCommandLineTest(unittest.TestCase):
    """Tests the database.py command line interface."""
    def setUp(self):
        self._dir = Path(tempfile.mkdtemp('_pw_tokenizer_test'))
        self._csv = self._dir / 'db.csv'
        self._elf = TOKENIZED_ENTRIES_ELF

        self._csv_test_domain = CSV_TEST_DOMAIN

    def tearDown(self):
        shutil.rmtree(self._dir)

    def test_create_csv(self):
        run_cli('create', '--database', self._csv, self._elf)

        self.assertEqual(CSV_DEFAULT_DOMAIN.splitlines(),
                         self._csv.read_text().splitlines())

    def test_create_csv_test_domain(self):
        run_cli('create', '--database', self._csv, f'{self._elf}#TEST_DOMAIN')

        self.assertEqual(self._csv_test_domain.splitlines(),
                         self._csv.read_text().splitlines())

    def test_create_csv_all_domains(self):
        run_cli('create', '--database', self._csv, f'{self._elf}#.*')

        self.assertEqual(CSV_ALL_DOMAINS.splitlines(),
                         self._csv.read_text().splitlines())

    def test_create_force(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        with self.assertRaises(FileExistsError):
            run_cli('create', '--database', self._csv, self._elf)

        run_cli('create', '--force', '--database', self._csv, self._elf)

    def test_create_binary(self):
        binary = self._dir / 'db.bin'
        run_cli('create', '--type', 'binary', '--database', binary, self._elf)

        # Write the binary database as CSV to verify its contents.
        run_cli('create', '--database', self._csv, binary)

        self.assertEqual(CSV_DEFAULT_DOMAIN.splitlines(),
                         self._csv.read_text().splitlines())

    def test_add_does_not_recalculate_tokens(self):
        db_with_custom_token = '01234567,          ,"hello"'

        to_add = self._dir / 'add_this.csv'
        to_add.write_text(db_with_custom_token + '\n')
        self._csv.touch()

        run_cli('add', '--database', self._csv, to_add)
        self.assertEqual(db_with_custom_token.splitlines(),
                         self._csv.read_text().splitlines())

    def test_mark_removals(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        run_cli('mark_removals', '--database', self._csv, '--date',
                '1998-09-04', self._elf)

        # Add the removal date to the four tokens not in the default domain
        new_csv = CSV_ALL_DOMAINS
        new_csv = new_csv.replace('17fa86d3,          ,"hello"',
                                  '17fa86d3,1998-09-04,"hello"')
        new_csv = new_csv.replace('18c5017c,          ,"yes"',
                                  '18c5017c,1998-09-04,"yes"')
        new_csv = new_csv.replace('59b2701c,          ,"The answer was: %s"',
                                  '59b2701c,1998-09-04,"The answer was: %s"')
        new_csv = new_csv.replace('d18ada0f,          ,"something"',
                                  'd18ada0f,1998-09-04,"something"')
        self.assertNotEqual(CSV_ALL_DOMAINS, new_csv)

        self.assertEqual(new_csv.splitlines(),
                         self._csv.read_text().splitlines())

    def test_purge(self):
        self._csv.write_text(CSV_ALL_DOMAINS)

        # Mark everything not in TEST_DOMAIN as removed.
        run_cli('mark_removals', '--database', self._csv,
                f'{self._elf}#TEST_DOMAIN')

        # Delete all entries except those in TEST_DOMAIN.
        run_cli('purge', '--database', self._csv)

        self.assertEqual(self._csv_test_domain.splitlines(),
                         self._csv.read_text().splitlines())

    @mock.patch('sys.stdout', new_callable=_mock_output)
    def test_report(self, mock_stdout):
        run_cli('report', self._elf)

        self.assertEqual(json.loads(mock_stdout.buffer.getvalue()),
                         EXPECTED_REPORT)

    def test_replace(self):
        sub = 'replace/ment'
        run_cli('create', '--database', self._csv, self._elf, '--replace',
                r'(?i)\b[jh]ello\b/' + sub)
        self.assertEqual(
            CSV_DEFAULT_DOMAIN.replace('Jello', sub).replace('Hello', sub),
            self._csv.read_text())


class LegacyDatabaseCommandLineTest(DatabaseCommandLineTest):
    """Test an ELF with the legacy plain string storage format."""
    def setUp(self):
        super().setUp()
        self._elf = LEGACY_PLAIN_STRING_ELF

        # The legacy approach for storing tokenized strings in an ELF always
        # adds an entry for "", even if the empty string was never tokenized.
        self._csv_test_domain = '00000000,          ,""\n' + CSV_TEST_DOMAIN

    @mock.patch('sys.stdout', new_callable=_mock_output)
    def test_report(self, mock_stdout):
        run_cli('report', self._elf)

        report = EXPECTED_REPORT[str(TOKENIZED_ENTRIES_ELF)].copy()

        # Count the implicitly added "" entry in TEST_DOMAIN.
        report['TEST_DOMAIN']['present_entries'] += 1
        report['TEST_DOMAIN']['present_size_bytes'] += 1
        report['TEST_DOMAIN']['total_entries'] += 1
        report['TEST_DOMAIN']['total_size_bytes'] += 1

        # Rename "" to the legacy name "default"
        report['default'] = report['']
        del report['']

        self.assertEqual({str(LEGACY_PLAIN_STRING_ELF): report},
                         json.loads(mock_stdout.buffer.getvalue()))


if __name__ == '__main__':
    unittest.main()
