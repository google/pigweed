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
"""Tests for the pw_build.gn_writer module."""

import os
import unittest

from io import StringIO
from pathlib import PurePath
from tempfile import TemporaryDirectory

from pw_build.gn_writer import (
    COPYRIGHT_HEADER,
    GnFile,
    GnWriter,
)
from pw_build.gn_target import GnTarget


class TestGnWriter(unittest.TestCase):
    """Tests for gn_writer.GnWriter."""

    def setUp(self):
        """Creates a GnWriter that writes to a StringIO."""
        self.output = StringIO()
        self.writer = GnWriter(self.output)

    def test_write_comment(self):
        """Writes a GN comment."""
        self.writer.write_comment('hello, world!')
        self.assertEqual(
            self.output.getvalue(),
            '# hello, world!\n',
        )

    def test_write_comment_wrap(self):
        """Writes a GN comment that is exactly 80 characters."""
        extra_long = (
            "This line is a " + ("really, " * 5) + "REALLY extra long comment"
        )
        self.writer.write_comment(extra_long)
        self.assertEqual(
            self.output.getvalue(),
            '# This line is a really, really, really, really, really, REALLY '
            'extra long\n# comment\n',
        )

    def test_write_comment_nowrap(self):
        """Writes a long GN comment without whitespace to wrap on."""
        no_breaks = 'A' + ('a' * 76) + 'h!'
        self.writer.write_comment(no_breaks)
        self.assertEqual(
            self.output.getvalue(),
            '# Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
            'aaaaaaaaaaaaah!\n',
        )

    def test_write_target(self):
        """Tests writing the target using a GnWriter."""
        target = GnTarget('custom_type', 'my-target')
        target.attrs = {
            'public': ['$src/foo/my-header.h'],
            'sources': ['$src/foo/my-source.cc'],
            'inputs': ['$src/bar/my.data'],
            'cflags': ['-frobinator'],
            'public_deps': [
                ':foo',
                '$dir_pw_third_party/repo/bar',
            ],
            'deps': [
                '$dir_pw_third_party/repo:top-level',
                '../another-pkg/baz',
            ],
        }
        target.origin = '//my-package:my-target'
        self.writer.write_target(target)

        self.assertEqual(
            self.output.getvalue(),
            '''
# Generated from //my-package:my-target
custom_type("my-target") {
  public = [
    "$src/foo/my-header.h",
  ]
  sources = [
    "$src/foo/my-source.cc",
  ]
  inputs = [
    "$src/bar/my.data",
  ]
  cflags = [
    "-frobinator",
  ]
  public_deps = [
    "$dir_pw_third_party/repo/bar",
    ":foo",
  ]
  deps = [
    "$dir_pw_third_party/repo:top-level",
    "../another-pkg/baz",
  ]
}
'''.lstrip(),
        )

    def test_write_list(self):
        """Writes a GN list assigned to a variable."""
        self.writer.write_list('empty', [])
        self.writer.write_list('items', ['foo', 'bar', 'baz'])
        lines = [
            'items = [',
            '  "bar",',
            '  "baz",',
            '  "foo",',
            ']',
        ]
        self.assertEqual('\n'.join(lines), self.output.getvalue().strip())

    def test_write_file(self):
        """Writes a BUILD.gn file."""
        imports = {
            '$dir_pw_third_party/foo/foo.gni',
            '$dir_pw_third_party/bar/bar.gni',
        }
        gn_targets = [
            GnTarget('foo_source_set', 'foo'),
            GnTarget('bar_source_set', 'bar'),
        ]
        gn_targets[0].attrs = {
            'public': ['$dir_pw_third_party_foo/foo.h'],
            'sources': ['$dir_pw_third_party_foo/foo.cc'],
            'deps': [':bar'],
        }
        gn_targets[1].attrs = {
            'public': ['$dir_pw_third_party_bar/bar.h'],
        }
        self.writer.write_file(imports, gn_targets)
        lines = [
            'import("//build_overrides/pigweed.gni")',
            '',
            'import("$dir_pw_third_party/bar/bar.gni")',
            'import("$dir_pw_third_party/foo/foo.gni")',
            '',
            'bar_source_set("bar") {',
            '  public = [',
            '    "$dir_pw_third_party_bar/bar.h",',
            '  ]',
            '}',
            '',
            'foo_source_set("foo") {',
            '  public = [',
            '    "$dir_pw_third_party_foo/foo.h",',
            '  ]',
            '  sources = [',
            '    "$dir_pw_third_party_foo/foo.cc",',
            '  ]',
            '  deps = [',
            '    ":bar",',
            '  ]',
            '}',
        ]
        self.assertEqual('\n'.join(lines), self.output.getvalue().strip())


class TestGnFile(unittest.TestCase):
    """Tests for gn_writer.GnFile."""

    def test_format_on_close(self):
        """Verifies the GN file is formatted when the file is closed."""
        with TemporaryDirectory() as tmpdirname:
            with GnFile(PurePath(tmpdirname, 'BUILD.gn')) as build_gn:
                build_gn.write('  correct = "indent"')
                build_gn.write_comment('comment')
                build_gn.write_list('single_item', ['is.inlined'])

            filename = PurePath('pw_build', 'gn_writer.py')
            expected = (
                COPYRIGHT_HEADER
                + f'''
# This file was automatically generated by {filename}

correct = "indent"

# comment
single_item = [ "is.inlined" ]
'''
            )
            with open(os.path.join(tmpdirname, 'BUILD.gn'), 'r') as build_gn:
                self.assertEqual(expected.strip(), build_gn.read().strip())


if __name__ == '__main__':
    unittest.main()
