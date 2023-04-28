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

from pw_build.gn_config import GnConfig
from pw_build.gn_writer import (
    COPYRIGHT_HEADER,
    GnFile,
    GnWriter,
)
from pw_build.gn_target import GnTarget
from pw_build.gn_utils import MalformedGnError


class TestGnWriter(unittest.TestCase):
    """Tests for gn_writer.GnWriter."""

    def setUp(self):
        """Creates a GnWriter that writes to a StringIO."""
        self.reset()

    def reset(self):
        """Resets the writer and output."""
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

    def test_write_imports(self):
        """Writes GN import statements."""
        self.writer.write_import('foo.gni')
        self.writer.write_imports(['bar.gni', 'baz.gni'])
        lines = [
            'import("foo.gni")',
            'import("bar.gni")',
            'import("baz.gni")',
        ]
        self.assertEqual('\n'.join(lines), self.output.getvalue().strip())

    def test_write_config(self):
        """Writes a GN config."""
        config = GnConfig(
            json='''{
            "label": "$dir_3p/test:my-config",
            "cflags": ["-frobinator", "-fizzbuzzer"],
            "defines": ["KEY=VAL"],
            "public": true,
            "usages": 1
        }'''
        )
        self.writer.write_config(config)
        lines = [
            'config("my-config") {',
            '  cflags = [',
            '    "-fizzbuzzer",',
            '    "-frobinator",',
            '  ]',
            '  defines = [',
            '    "KEY=VAL",',
            '  ]',
            '}',
        ]
        self.assertEqual('\n'.join(lines), self.output.getvalue().strip())

    def test_write_target(self):
        """Tests writing the target using a GnWriter."""
        target = GnTarget(
            '$build',
            '$src',
            json='''{
              "target_type": "custom_type",
              "target_name": "my-target",
              "package": "my-package"
            }''',
        )
        target.add_visibility(bazel='//visibility:private')
        target.add_visibility(bazel='//foo:__subpackages__')
        target.add_path('public', bazel='//foo:my-header.h')
        target.add_path('sources', bazel='//foo:my-source.cc')
        target.add_path('inputs', bazel='//bar:my.data')
        target.config.add('cflags', '-frobinator')
        target.add_dep(public=True, bazel='//my-package:foo')
        target.add_dep(public=True, bazel='@com_corp_repo//bar')
        target.add_dep(bazel='//other-pkg/baz')
        target.add_dep(bazel='@com_corp_repo//:top-level')

        output = StringIO()
        writer = GnWriter(output)
        writer.repos = {'com_corp_repo': 'repo'}
        writer.aliases = {'$build/other-pkg/baz': '$build/another-pkg/baz'}
        writer.write_target(target)

        self.assertEqual(
            output.getvalue(),
            '''
# Generated from //my-package:my-target
custom_type("my-target") {
  visibility = [
    "../foo/*",
    ":*",
  ]
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

    def test_write_target_public_visibility(self):
        """Tests writing a globbaly visible target using a GnWriter."""
        target = GnTarget(
            '$build',
            '$src',
            json='''{
              "target_type": "custom_type",
              "target_name": "my-target",
              "package": "my-package"
            }''',
        )
        target.add_visibility(bazel='//visibility:private')
        target.add_visibility(bazel='//visibility:public')

        output = StringIO()
        writer = GnWriter(output)
        writer.repos = {'com_corp_repo': 'repo'}
        writer.aliases = {'$build/other-pkg/baz': '$build/another-pkg/baz'}
        writer.write_target(target)

        self.assertEqual(
            output.getvalue(),
            '''
# Generated from //my-package:my-target
custom_type("my-target") {
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

    def test_write_scope(self):
        """Writes a GN scope assigned to a variable."""
        self.writer.write_scope('outer')
        self.writer.write('key1 = "val1"')
        self.writer.write_scope('inner')
        self.writer.write('key2 = "val2"')
        self.writer.write_end()
        self.writer.write('key3 = "val3"')
        self.writer.write_end()
        lines = [
            'outer = {',
            '  key1 = "val1"',
            '  inner = {',
            '    key2 = "val2"',
            '  }',
            '',
            '  key3 = "val3"',
            '}',
        ]
        self.assertEqual('\n'.join(lines), self.output.getvalue().strip())

    def test_write_if_else_end(self):
        """Writes GN conditional statements."""
        self.writer.write_if('current_os == "linux"')
        self.writer.write('mascot = "penguin"')
        self.writer.write_else_if('current_os == "mac"')
        self.writer.write('mascot = "dogcow"')
        self.writer.write_else_if('current_os == "win"')
        self.writer.write('mascot = "clippy"')
        self.writer.write_else()
        self.writer.write('mascot = "dropbear"')
        self.writer.write_end()
        lines = [
            'if (current_os == "linux") {',
            '  mascot = "penguin"',
            '} else if (current_os == "mac") {',
            '  mascot = "dogcow"',
            '} else if (current_os == "win") {',
            '  mascot = "clippy"',
            '} else {',
            '  mascot = "dropbear"',
            '}',
        ]
        self.assertEqual('\n'.join(lines), self.output.getvalue().strip())

    def test_write_unclosed_target(self):
        """Triggers an error from an unclosed GN scope."""
        self.writer.write_target_start('unclosed', 'target')
        with self.assertRaises(MalformedGnError):
            self.writer.seal()

    def test_write_unclosed_scope(self):
        """Triggers an error from an unclosed GN scope."""
        self.writer.write_scope('unclosed_scope')
        with self.assertRaises(MalformedGnError):
            self.writer.seal()

    def test_write_unclosed_if(self):
        """Triggers an error from an unclosed GN condition."""
        self.writer.write_if('var == "unclosed-if"')
        with self.assertRaises(MalformedGnError):
            self.writer.seal()

    def test_write_unclosed_else_if(self):
        """Triggers an error from an unclosed GN condition."""
        self.writer.write_if('var == "closed-if"')
        self.writer.write_else_if('var == "unclosed-else-if"')
        with self.assertRaises(MalformedGnError):
            self.writer.seal()

    def test_write_unclosed_else(self):
        """Triggers an error from an unclosed GN condition."""
        self.writer.write_if('var == "closed-if"')
        self.writer.write_else_if('var == "closed-else-if"')
        self.writer.write_else()
        with self.assertRaises(MalformedGnError):
            self.writer.seal()


class TestGnFile(unittest.TestCase):
    """Tests for gn_writer.GnFile."""

    def test_format_on_close(self):
        """Verifies the GN file is formatted when the file is closed."""
        with TemporaryDirectory() as tmpdirname:
            with GnFile(PurePath(tmpdirname, 'BUILD.gn')) as build_gn:
                build_gn.write('  correct = "indent"')
                build_gn.write_comment('newline before comment')
                build_gn.write_scope('no_newline_before_item')
                build_gn.write_list('single_item', ['is.inlined'])
                build_gn.write_end()

            filename = PurePath('pw_build', 'gn_writer.py')
            expected = (
                COPYRIGHT_HEADER
                + f'''
# This file was automatically generated by {filename}

correct = "indent"

# newline before comment
no_newline_before_item = {{
  single_item = [ "is.inlined" ]
}}'''
            )
            with open(os.path.join(tmpdirname, 'BUILD.gn'), 'r') as build_gn:
                self.assertEqual(expected.strip(), build_gn.read().strip())


if __name__ == '__main__':
    unittest.main()
