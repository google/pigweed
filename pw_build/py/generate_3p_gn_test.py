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
"""Tests for the pw_build.generate_3p_gn module."""

import unittest

from contextlib import AbstractContextManager
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock
from types import TracebackType
from typing import Iterator, Optional, Type

from pw_build.generate_3p_gn import GnGenerator, write_owners
from pw_build.gn_config import GnConfig
from pw_build.gn_writer import GnWriter


class GnGeneratorForTest(AbstractContextManager):
    """Test fixture that creates a generator for a temporary directory."""

    def __init__(self):
        self._tmp = TemporaryDirectory()

    def __enter__(self) -> GnGenerator:
        """Creates a temporary directory and uses it to create a generator."""
        tmp = self._tmp.__enter__()
        generator = GnGenerator()
        path = Path(tmp) / 'repo'
        path.mkdir(parents=True)
        generator.load_workspace(path)
        return generator

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> None:
        """Removes the temporary directory."""
        self._tmp.__exit__(exc_type, exc_val, exc_tb)


def mock_return_values(mock_run, retvals: Iterator[str]) -> None:
    """Mocks the return values of several calls to subprocess.run."""
    side_effects = []
    for retval in retvals:
        attr = {'stdout.decode.return_value': retval}
        side_effects.append(mock.MagicMock(**attr))
    mock_run.side_effect = side_effects


class TestGenerator(unittest.TestCase):
    """Tests for generate_3p_gn.GnGenerator."""

    def test_generate_configs(self):
        """Tests finding the most common configs."""
        generator = GnGenerator()
        generator.set_repo('test')

        generator.add_target(
            json='''{
                "target_name": "target0",
                "cflags": ["common"]
            }''',
        )

        generator.add_target(
            json='''{
                "target_name": "target1",
                "package": "foo",
                "include_dirs": ["foo"],
                "cflags": ["common", "foo-flag1"]
            }''',
        )

        generator.add_target(
            json='''{
                "target_name": "target2",
                "package": "foo",
                "include_dirs": ["foo"],
                "cflags": ["common", "foo-flag1", "foo-flag2"]
            }''',
        )

        generator.add_target(
            json='''{
                "target_name": "target3",
                "package": "foo",
                "include_dirs": ["foo"],
                "cflags": ["common", "foo-flag1"]
            }''',
        )

        generator.add_target(
            json='''{
                "target_name": "target4",
                "package": "bar",
                "include_dirs": ["bar"],
                "cflags": ["common", "bar-flag"]
            }''',
        )

        configs_to_add = ['//:added']
        configs_to_remove = ['//remove:me']
        generator.generate_configs(configs_to_add, configs_to_remove)

        self.assertEqual(
            generator.configs[''],
            [
                GnConfig(
                    json='''{
                      "label": "$dir_pw_third_party/test:test_config1",
                      "cflags": ["common"],
                      "usages": 5
                    }'''
                )
            ],
        )

        self.assertEqual(
            generator.configs['foo'],
            [
                GnConfig(
                    json='''{
                      "label": "$dir_pw_third_party/test/foo:foo_config1",
                      "cflags": ["foo-flag1"],
                      "public": false,
                      "usages": 3
                    }'''
                ),
                GnConfig(
                    json='''{
                      "label": "$dir_pw_third_party/test/foo:foo_public_config1",
                      "include_dirs": ["foo"],
                      "public": true,
                      "usages": 3
                    }'''
                ),
            ],
        )

        self.assertEqual(
            generator.configs['bar'],
            [
                GnConfig(
                    json='''{
                      "label": "$dir_pw_third_party/test/bar:bar_public_config1",
                      "include_dirs": ["bar"],
                      "public": true,
                      "usages": 1
                    }'''
                )
            ],
        )

        targets = [
            target
            for targets in generator.targets.values()
            for target in targets
        ]
        targets.sort(key=lambda target: target.name())

        target0 = targets[0]
        self.assertFalse(target0.config)
        self.assertEqual(
            target0.configs,
            {'//:added', '$dir_pw_third_party/test:test_config1'},
        )
        self.assertFalse(target0.public_configs)
        self.assertEqual(target0.remove_configs, {'//remove:me'})

        target1 = targets[1]
        self.assertFalse(target1.config)
        self.assertEqual(
            target1.configs,
            {
                '//:added',
                '$dir_pw_third_party/test:test_config1',
                '$dir_pw_third_party/test/foo:foo_config1',
            },
        )
        self.assertEqual(
            target1.public_configs,
            {'$dir_pw_third_party/test/foo:foo_public_config1'},
        )
        self.assertEqual(target1.remove_configs, {'//remove:me'})

        target2 = targets[2]
        self.assertEqual(
            target2.config,
            GnConfig(
                json='''{
                  "cflags": ["foo-flag2"],
                  "public": false,
                  "usages": 0
                }'''
            ),
        )
        self.assertEqual(
            target2.configs,
            {
                '//:added',
                '$dir_pw_third_party/test:test_config1',
                '$dir_pw_third_party/test/foo:foo_config1',
            },
        )
        self.assertEqual(
            target2.public_configs,
            {'$dir_pw_third_party/test/foo:foo_public_config1'},
        )
        self.assertEqual(target2.remove_configs, {'//remove:me'})

        target3 = targets[3]
        self.assertFalse(target3.config)
        self.assertEqual(
            target3.configs,
            {
                '//:added',
                '$dir_pw_third_party/test:test_config1',
                '$dir_pw_third_party/test/foo:foo_config1',
            },
        )
        self.assertEqual(
            target3.public_configs,
            {'$dir_pw_third_party/test/foo:foo_public_config1'},
        )
        self.assertEqual(target3.remove_configs, {'//remove:me'})

        target4 = targets[4]
        self.assertEqual(
            target4.config,
            GnConfig(
                json='''{
                  "cflags": ["bar-flag"],
                  "public": false,
                  "usages": 0
                }'''
            ),
        )
        self.assertEqual(
            target4.configs,
            {'//:added', '$dir_pw_third_party/test:test_config1'},
        )
        self.assertEqual(
            target4.public_configs,
            {'$dir_pw_third_party/test/bar:bar_public_config1'},
        )
        self.assertEqual(target4.remove_configs, {'//remove:me'})

    def test_write_build_gn(self):
        """Tests writing a complete BUILD.gn file."""
        generator = GnGenerator()
        generator.set_repo('test')
        generator.exclude_from_gn_check(bazel='//bar:target3')

        generator.add_configs(
            '',
            GnConfig(
                json='''{
                  "label": "$dir_pw_third_party/test:test_config1",
                  "cflags": ["common"],
                  "usages": 5
                }'''
            ),
        )

        generator.add_configs(
            'foo',
            GnConfig(
                json='''{
                  "label": "$dir_pw_third_party/test/foo:foo_config1",
                  "cflags": ["foo-flag1"],
                  "public": false,
                  "usages": 3
                }'''
            ),
            GnConfig(
                json='''{
                  "label": "$dir_pw_third_party/test/foo:foo_public_config1",
                  "include_dirs": ["foo"],
                  "public": true,
                  "usages": 3
                }'''
            ),
        )

        generator.add_configs(
            'bar',
            GnConfig(
                json='''{
                  "label": "$dir_pw_third_party/test/bar:bar_public_config1",
                  "include_dirs": ["bar"],
                  "public": true,
                  "usages": 1
                }'''
            ),
        )

        generator.add_target(
            json='''{
                "target_type": "pw_executable",
                "target_name": "target0",
                "configs": ["$dir_pw_third_party/test:test_config1"],
                "sources": ["$dir_pw_third_party_test/target0.cc"],
                "deps": [
                    "$dir_pw_third_party/test/foo:target1",
                    "$dir_pw_third_party/test/foo:target2"
                ]
            }''',
        )

        generator.add_target(
            json='''{
                "target_type": "pw_source_set",
                "target_name": "target1",
                "package": "foo",
                "public": ["$dir_pw_third_party_test/foo/target1.h"],
                "sources": ["$dir_pw_third_party_test/foo/target1.cc"],
                "public_configs": ["$dir_pw_third_party/test/foo:foo_public_config1"],
                "configs": [
                    "$dir_pw_third_party/test:test_config1",
                    "$dir_pw_third_party/test/foo:foo_config1"
                ]
            }''',
        )

        generator.add_target(
            json='''{
                "target_type": "pw_source_set",
                "target_name": "target2",
                "package": "foo",
                "sources": ["$dir_pw_third_party_test/foo/target2.cc"],
                "public_configs": ["$dir_pw_third_party/test/foo:foo_public_config1"],
                "configs": [
                    "$dir_pw_third_party/test:test_config1",
                    "$dir_pw_third_party/test/foo:foo_config1"
                ],
                "cflags": ["foo-flag2"],
                "public_deps": ["$dir_pw_third_party/test/bar:target3"]
            }''',
        )

        generator.add_target(
            json='''{
                "target_type": "pw_source_set",
                "target_name": "target3",
                "package": "bar",
                "include_dirs": ["bar"],
                "public_configs": ["$dir_pw_third_party/test/bar:bar_public_config1"],
                "configs": ["$dir_pw_third_party/test:test_config1"],
                "cflags": ["bar-flag"]
            }''',
        )

        output = StringIO()
        build_gn = GnWriter(output)
        generator.write_build_gn('', build_gn)
        self.assertEqual(
            output.getvalue(),
            '''
import("//build_overrides/pigweed.gni")

import("$dir_pw_build/target_types.gni")
import("$dir_pw_docgen/docs.gni")
import("$dir_pw_third_party/test/test.gni")

if (dir_pw_third_party_test != "") {
  config("test_config1") {
    cflags = [
      "common",
    ]
  }

  # Generated from //:target0
  pw_executable("target0") {
    sources = [
      "$dir_pw_third_party_test/target0.cc",
    ]
    configs = [
      ":test_config1",
    ]
    deps = [
      "foo:target1",
      "foo:target2",
    ]
  }
}

pw_doc_group("docs") {
  sources = [
    "docs.rst",
  ]
}
'''.lstrip(),
        )

        output = StringIO()
        build_gn = GnWriter(output)
        generator.write_build_gn('foo', build_gn)
        self.assertEqual(
            output.getvalue(),
            '''
import("//build_overrides/pigweed.gni")

import("$dir_pw_build/target_types.gni")
import("$dir_pw_third_party/test/test.gni")

config("foo_public_config1") {
  include_dirs = [
    "foo",
  ]
}

config("foo_config1") {
  cflags = [
    "foo-flag1",
  ]
}

# Generated from //foo:target1
pw_source_set("target1") {
  public = [
    "$dir_pw_third_party_test/foo/target1.h",
  ]
  sources = [
    "$dir_pw_third_party_test/foo/target1.cc",
  ]
  public_configs = [
    ":foo_public_config1",
  ]
  configs = [
    "..:test_config1",
    ":foo_config1",
  ]
}

# Generated from //foo:target2
pw_source_set("target2") {
  sources = [
    "$dir_pw_third_party_test/foo/target2.cc",
  ]
  cflags = [
    "foo-flag2",
  ]
  public_configs = [
    ":foo_public_config1",
  ]
  configs = [
    "..:test_config1",
    ":foo_config1",
  ]
  public_deps = [
    "../bar:target3",
  ]
}
'''.lstrip(),
        )

        output = StringIO()
        build_gn = GnWriter(output)
        generator.write_build_gn('bar', build_gn)
        self.assertEqual(
            output.getvalue(),
            '''
import("//build_overrides/pigweed.gni")

import("$dir_pw_build/target_types.gni")
import("$dir_pw_third_party/test/test.gni")

config("bar_public_config1") {
  include_dirs = [
    "bar",
  ]
}

# Generated from //bar:target3
pw_source_set("target3") {
  check_includes = false
  cflags = [
    "bar-flag",
  ]
  include_dirs = [
    "bar",
  ]
  public_configs = [
    ":bar_public_config1",
  ]
  configs = [
    "..:test_config1",
  ]
}
'''.lstrip(),
        )

    def test_write_repo_gni(self):
        """Tests writing the GN import file for a repo."""
        output = StringIO()
        with GnGeneratorForTest() as generator:
            generator.write_repo_gni(GnWriter(output), 'Repo')

        self.assertEqual(
            output.getvalue(),
            '''
declare_args() {
  # If compiling tests with Repo, this variable is set to the path to the Repo
  # installation. When set, a pw_source_set for the Repo library is created at
  # "$dir_pw_third_party/repo".
  dir_pw_third_party_repo = ""
}
'''.lstrip(),
        )

    @mock.patch('subprocess.run')
    def test_write_docs_rst(self, mock_run):
        """Tests writing the reStructuredText docs for a repo."""
        mock_return_values(
            mock_run,
            [
                'https://host/repo.git',
                'https://host/repo.git',
                'deadbeeffeedface',
            ],
        )
        output = StringIO()
        with GnGeneratorForTest() as generator:
            generator.write_docs_rst(output, 'Repo')

        self.assertEqual(
            output.getvalue(),
            '''
.. _module-pw_third_party_repo:

====
Repo
====
The ``$dir_pw_third_party/repo/`` module provides build files to allow
optionally including upstream Repo.

-------------------
Using upstream Repo
-------------------
If you want to use Repo, you must do the following:

Submodule
=========
Add Repo to your workspace with the following command.

.. code-block:: sh

  git submodule add https://host/repo.git \\
    third_party/repo/src

GN
==
* Set the GN var ``dir_pw_third_party_repo`` to the location of the
  Repo source.

  If you used the command above, this will be
  ``//third_party/repo/src``

  This can be set in your args.gn or .gn file like:
  ``dir_pw_third_party_repo = "//third_party/repo/src"``

Updating
========
The GN build files are generated from the third-party Bazel build files using
$dir_pw_build/py/pw_build/generate_3p_gn.py.

The script uses data taken from ``$dir_pw_third_party/repo/repo.json``.
The schema of ``repo.json`` is described in :ref:`module-pw_build-third-party`.

The script should be re-run whenever the submodule is updated or the JSON file
is modified. Specify the location of the Bazel repository can be specified using
the ``-w`` option, e.g.

.. code-block:: sh

  python pw_build/py/pw_build/generate_3p_gn.py \\
    -w third_party/repo/src

.. DO NOT EDIT BELOW THIS LINE. Generated section.

Version
=======
The update script was last run for revision `deadbeef`_.

.. _deadbeef: https://host/repo/tree/deadbeeffeedface
'''.lstrip(),
        )

    @mock.patch('subprocess.run')
    def test_update_docs_rst_same_rev(self, mock_run):
        """Tests updating the docs with the same revision."""
        mock_return_values(
            mock_run,
            [
                'https://host/repo.git',
                'https://host/repo.git',
                'deadbeeffeedface',
                'https://host/repo.git',
                'deadbeeffeedface',
            ],
        )
        output = StringIO()
        with GnGeneratorForTest() as generator:
            generator.write_docs_rst(output, 'Repo')
            original = output.getvalue().split('\n')
            updated = list(generator.update_version(original))

        self.assertEqual(original, updated)

    @mock.patch('subprocess.run')
    def test_update_docs_rst_new_rev(self, mock_run):
        """Tests updating the docs with the different revision."""
        mock_return_values(
            mock_run,
            [
                'https://host/repo.git',
                'https://host/repo.git',
                'deadbeeffeedface',
                'https://host/repo.git',
                '0123456789abcdef',
            ],
        )
        output = StringIO()
        with GnGeneratorForTest() as generator:
            generator.write_docs_rst(output, 'Repo')
            contents = output.getvalue()
            original = contents.split('\n')

            # Convert the contents to a list of lines similar to those returned
            # by iterating over an open file. In particular, include a newline
            # at the end of each line.
            with_newlines = [s + '\n' for s in original]
            updated = list(generator.update_version(with_newlines))

        self.assertEqual(original[:-6], updated[:-6])
        self.assertEqual(
            '\n'.join(updated[-6:]),
            '''
Version
=======
The update script was last run for revision `01234567`_.

.. _01234567: https://host/repo/tree/0123456789abcdef
'''.lstrip(),
        )

    @mock.patch('subprocess.run')
    def test_update_docs_rst_no_rev(self, mock_run):
        """Tests updating docs that do not have a revision."""
        mock_return_values(
            mock_run,
            [
                'https://host/repo.git',
                '0123456789abcdef',
            ],
        )
        with GnGeneratorForTest() as generator:
            updated = list(generator.update_version(['foo', 'bar', '']))

        self.assertEqual(
            '\n'.join(updated),
            '''
foo
bar

.. DO NOT EDIT BELOW THIS LINE. Generated section.

Version
=======
The update script was last run for revision `01234567`_.

.. _01234567: https://host/repo/tree/0123456789abcdef
'''.lstrip(),
        )

    def test_update_third_party_docs(self):
        """Tests adding docs to //docs::third_party_docs."""
        with GnGeneratorForTest() as generator:
            contents = generator.update_third_party_docs(
                '''
group("third_party_docs") {
  deps = [
    "$dir_pigweed/third_party/existing:docs",
  ]
}
'''
            )
        # Formatting is performed separately.
        self.assertEqual(
            contents,
            '''
group("third_party_docs") {
deps = ["$dir_pigweed/third_party/repo:docs",
    "$dir_pigweed/third_party/existing:docs",
  ]
}
''',
        )

    def test_update_third_party_docs_no_target(self):
        """Tests adding docs to a file without a "third_party_docs" target."""
        with GnGeneratorForTest() as generator:
            with self.assertRaises(ValueError):
                generator.update_third_party_docs('')

    @mock.patch('subprocess.run')
    def test_write_extra(self, mock_run):
        """Tests extra files produced via `bazel run`."""
        attr = {'stdout.decode.return_value': 'hello, world!'}
        mock_run.return_value = mock.MagicMock(**attr)

        output = StringIO()
        with GnGeneratorForTest() as generator:
            generator.write_extra(output, 'some_label')
        self.assertEqual(output.getvalue(), 'hello, world!')

    @mock.patch('subprocess.run')
    def test_write_owners(self, mock_run):
        """Tests writing an OWNERS file."""
        attr = {'stdout.decode.return_value': 'someone@pigweed.dev'}
        mock_run.return_value = mock.MagicMock(**attr)

        output = StringIO()
        write_owners(output)
        self.assertEqual(output.getvalue(), 'someone@pigweed.dev')


if __name__ == '__main__':
    unittest.main()
