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
"""Tests for argument_types."""

import argparse
from collections import defaultdict
import logging
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

# pylint: disable=unused-import

# Mocked/patched imports.
import sys

# pylint: enable=unused-import

from pw_cli.argument_types import (
    directory,
    log_level,
    DictOfListsAction,
)


class TestDictOfListsAction(unittest.TestCase):
    """Tests for DictOfListsAction."""

    def setUp(self):
        self.parser = argparse.ArgumentParser()

    def test_valid_args(self):
        """Test parsing valid arguments."""
        self.parser.add_argument(
            '--my-arg', '-A', action=DictOfListsAction, dest='my_dest'
        )
        args = self.parser.parse_args(
            [
                '--my-arg',
                'key1=val1',
                '--my-arg',
                'key2=val2',
                '-Afoo=bar',
                '--my-arg=foo=baz',
                '-A=foo=qux',
            ]
        )
        self.assertEqual(
            args.my_dest,
            {'key1': ['val1'], 'key2': ['val2'], 'foo': ['bar', 'baz', 'qux']},
        )

    def test_repeated_key(self):
        """Test repeating the same key."""
        self.parser.add_argument(
            '--my-arg', action=DictOfListsAction, dest='my_dest'
        )
        args = self.parser.parse_args(
            ['--my-arg', 'key1=val1', '--my-arg', 'key1=val2']
        )
        self.assertEqual(args.my_dest, {'key1': ['val1', 'val2']})

    def test_no_default(self):
        """Test that the destination is None if not provided."""
        self.parser.add_argument(
            '--my-arg', action=DictOfListsAction, dest='my_dest'
        )
        args = self.parser.parse_args([])
        self.assertIsNone(args.my_dest)

    def test_with_default(self):
        """Test providing a default value."""
        default_dict = defaultdict(list, {'default': ['value']})
        self.parser.add_argument(
            '--my-arg',
            action=DictOfListsAction,
            dest='my_dest',
            default=default_dict,
        )
        args = self.parser.parse_args([])
        self.assertEqual(args.my_dest, default_dict)

    def test_invalid_arg_format(self):
        """Test error handling for invalid argument format."""
        self.parser.add_argument('--my-arg', action=DictOfListsAction)
        with self.assertRaises(SystemExit):
            with patch('sys.stderr'):  # Suppress argparse error message
                self.parser.parse_args(['--my-arg', 'key1'])

    def test_metavar_default(self):
        """Test the default metavar."""
        action = self.parser.add_argument('--my-arg', action=DictOfListsAction)
        self.assertEqual(action.metavar, 'KEY=VALUE')

    def test_metavar_from_string(self):
        """Test setting metavar from a string."""
        action = self.parser.add_argument(
            '--my-arg', action=DictOfListsAction, metavar='MYKEY=MYVAL'
        )
        self.assertEqual(action.metavar, 'MYKEY=MYVAL')

    def test_metavar_from_tuple(self):
        """Test setting metavar from a tuple."""
        action = self.parser.add_argument(
            '--my-arg', action=DictOfListsAction, metavar=('MYKEY', 'MYVAL')
        )
        self.assertEqual(action.metavar, 'MYKEY=MYVAL')

    def test_invalid_metavar_string_no_equals(self):
        """Test error on string metavar without '='."""
        with self.assertRaisesRegex(ValueError, 'contain exactly one "="'):
            self.parser.add_argument(
                '--my-arg', action=DictOfListsAction, metavar='KEYVAL'
            )

    def test_invalid_metavar_string_too_many_equals(self):
        """Test error on string metavar with multiple '='."""
        with self.assertRaisesRegex(ValueError, 'contain exactly one "="'):
            self.parser.add_argument(
                '--my-arg', action=DictOfListsAction, metavar='KEY=V=AL'
            )

    def test_invalid_metavar_tuple_too_short(self):
        """Test error on tuple metavar with too few items."""
        with self.assertRaisesRegex(ValueError, 'must have two strings'):
            self.parser.add_argument(
                '--my-arg', action=DictOfListsAction, metavar=('KEY',)
            )

    def test_invalid_metavar_tuple_too_long(self):
        """Test error on tuple metavar with too many items."""
        with self.assertRaisesRegex(ValueError, 'must have two strings'):
            self.parser.add_argument(
                '--my-arg',
                action=DictOfListsAction,
                metavar=('KEY', 'VALUE', 'EXTRA'),
            )

    def test_invalid_metavar_type(self):
        """Test error on invalid metavar type."""
        with self.assertRaisesRegex(ValueError, 'must be a string or a tuple'):
            self.parser.add_argument(
                '--my-arg', action=DictOfListsAction, metavar=123
            )

    def test_nargs_not_allowed(self):
        """Test that using nargs raises an error."""
        with self.assertRaisesRegex(ValueError, 'nargs is not allowed'):
            self.parser.add_argument(
                '--my-arg', action=DictOfListsAction, nargs='+'
            )


class TestDirectoryType(unittest.TestCase):
    """Tests for the directory argument type."""

    def test_valid_directory(self):
        """Test with a valid directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir)
            self.assertEqual(directory(str(path)), path.resolve())

    def test_nonexistent_directory(self):
        """Test with a path that doesn't exist."""
        with self.assertRaises(argparse.ArgumentTypeError):
            directory('nonexistent_directory_for_testing')

    def test_file_instead_of_directory(self):
        """Test with a path that is a file."""
        with tempfile.NamedTemporaryFile() as tmpfile:
            with self.assertRaises(argparse.ArgumentTypeError):
                directory(tmpfile.name)


class TestLogLevelType(unittest.TestCase):
    """Tests for the log_level argument type."""

    def test_valid_log_levels(self):
        """Test with valid log level strings."""
        self.assertEqual(log_level('DEBUG'), logging.DEBUG)
        self.assertEqual(log_level('info'), logging.INFO)
        self.assertEqual(log_level('WARNING'), logging.WARNING)
        self.assertEqual(log_level('error'), logging.ERROR)
        self.assertEqual(log_level('CRITICAL'), logging.CRITICAL)

    def test_invalid_log_level(self):
        """Test with an invalid log level string."""
        with self.assertRaises(argparse.ArgumentTypeError):
            log_level('INVALID_LOG_LEVEL')


if __name__ == '__main__':
    unittest.main()
