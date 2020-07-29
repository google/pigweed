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
"""Tests for the Python runner."""

import os
from pathlib import Path
import tempfile
import unittest

from pw_build.python_runner import GnPaths, Label, TargetInfo

TEST_PATHS = GnPaths(Path('/gn_root'), Path('/gn_root/out'),
                     Path('/gn_root/some/cwd'), '//toolchains/cool:ToolChain')


class LabelTest(unittest.TestCase):
    """Tests GN label parsing."""
    def setUp(self):
        self._paths_and_toolchain_name = [
            (TEST_PATHS, 'ToolChain'),
            (GnPaths(*TEST_PATHS[:3], ''), ''),
        ]

    def test_root(self):
        for paths, toolchain in self._paths_and_toolchain_name:
            label = Label(paths, '//')
            self.assertEqual(label.name, '')
            self.assertEqual(label.dir, Path('/gn_root'))
            self.assertEqual(label.out_dir,
                             Path('/gn_root/out', toolchain, 'obj'))
            self.assertEqual(label.gen_dir,
                             Path('/gn_root/out', toolchain, 'gen'))

    def test_absolute(self):
        for paths, toolchain in self._paths_and_toolchain_name:
            label = Label(paths, '//foo/bar:baz')
            self.assertEqual(label.name, 'baz')
            self.assertEqual(label.dir, Path('/gn_root/foo/bar'))
            self.assertEqual(label.out_dir,
                             Path('/gn_root/out', toolchain, 'obj/foo/bar'))
            self.assertEqual(label.gen_dir,
                             Path('/gn_root/out', toolchain, 'gen/foo/bar'))

    def test_absolute_implicit_target(self):
        for paths, toolchain in self._paths_and_toolchain_name:
            label = Label(paths, '//foo/bar')
            self.assertEqual(label.name, 'bar')
            self.assertEqual(label.dir, Path('/gn_root/foo/bar'))
            self.assertEqual(label.out_dir,
                             Path('/gn_root/out', toolchain, 'obj/foo/bar'))
            self.assertEqual(label.gen_dir,
                             Path('/gn_root/out', toolchain, 'gen/foo/bar'))

    def test_relative(self):
        for paths, toolchain in self._paths_and_toolchain_name:
            label = Label(paths, ':tgt')
            self.assertEqual(label.name, 'tgt')
            self.assertEqual(label.dir, Path('/gn_root/some/cwd'))
            self.assertEqual(label.out_dir,
                             Path('/gn_root/out', toolchain, 'obj/some/cwd'))
            self.assertEqual(label.gen_dir,
                             Path('/gn_root/out', toolchain, 'gen/some/cwd'))

    def test_relative_subdir(self):
        for paths, toolchain in self._paths_and_toolchain_name:
            label = Label(paths, 'tgt')
            self.assertEqual(label.name, 'tgt')
            self.assertEqual(label.dir, Path('/gn_root/some/cwd/tgt'))
            self.assertEqual(
                label.out_dir,
                Path('/gn_root/out', toolchain, 'obj/some/cwd/tgt'))
            self.assertEqual(
                label.gen_dir,
                Path('/gn_root/out', toolchain, 'gen/some/cwd/tgt'))

    def test_relative_parent_dir(self):
        for paths, toolchain in self._paths_and_toolchain_name:
            label = Label(paths, '..:tgt')
            self.assertEqual(label.name, 'tgt')
            self.assertEqual(label.dir, Path('/gn_root/some'))
            self.assertEqual(label.out_dir,
                             Path('/gn_root/out', toolchain, 'obj/some'))
            self.assertEqual(label.gen_dir,
                             Path('/gn_root/out', toolchain, 'gen/some'))


class ResolvePathTest(unittest.TestCase):
    """Tests GN path resolution."""
    def test_resolve_absolute(self):
        self.assertEqual(TEST_PATHS.resolve('//'), TEST_PATHS.root)
        self.assertEqual(TEST_PATHS.resolve('//foo/bar'),
                         TEST_PATHS.root / 'foo' / 'bar')
        self.assertEqual(TEST_PATHS.resolve('//foo/../baz'),
                         TEST_PATHS.root / 'baz')

    def test_resolve_relative(self):
        self.assertEqual(TEST_PATHS.resolve(''), TEST_PATHS.cwd)
        self.assertEqual(TEST_PATHS.resolve('foo'), TEST_PATHS.cwd / 'foo')
        self.assertEqual(TEST_PATHS.resolve('..'), TEST_PATHS.root / 'some')


NINJA_EXECUTABLE = '''\
defines =
framework_dirs =
include_dirs = -I../fake_module/public
cflags = -g3 -Og -fdiagnostics-color -g -fno-common -Wall -Wextra -Werror
cflags_c =
cflags_cc = -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register
target_output_name = this_is_a_test

build fake_toolchain/obj/fake_module/fake_test.fake_test.cc.o: fake_toolchain_cxx ../fake_module/fake_test.cc
build fake_toolchain/obj/fake_module/fake_test.fake_test_c.c.o: fake_toolchain_cc ../fake_module/fake_test_c.c

build fake_toolchain/obj/fake_module/test/fake_test.elf: fake_tolchain_link fake_tolchain/obj/fake_module/fake_test.fake_test.cc.o fake_tolchain/obj/fake_module/fake_test.fake_test_c.c.o
  ldflags = -Og -fdiagnostics-color
  libs =
  frameworks =
  output_extension =
  output_dir = host_clang_debug/obj/fake_module/test
'''

NINJA_SOURCE_SET = '''\
defines =
framework_dirs =
include_dirs = -I../fake_module/public
cflags = -g3 -Og -fdiagnostics-color -g -fno-common -Wall -Wextra -Werror
cflags_c =
cflags_cc = -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register
target_output_name = this_is_a_test

build fake_toolchain/obj/fake_module/fake_source_set.file_a.cc.o: fake_toolchain_cxx ../fake_module/file_a.cc
build fake_toolchain/obj/fake_module/fake_source_set.file_b.c.o: fake_toolchain_cc ../fake_module/file_b.c

build fake_toolchain/obj/fake_module/fake_source_set.stamp: fake_tolchain_link fake_tolchain/obj/fake_module/fake_source_set.file_a.cc.o fake_tolchain/obj/fake_module/fake_source_set.file_b.c.o
  ldflags = -Og -fdiagnostics-color -Wno-error=deprecated
  libs =
  frameworks =
  output_extension =
  output_dir = host_clang_debug/obj/fake_module
'''


class TargetTest(unittest.TestCase):
    """Tests querying GN target information."""
    def setUp(self):
        self._tempdir = tempfile.TemporaryDirectory(prefix='pw_build_test')
        self._paths = GnPaths(root=Path(self._tempdir.name),
                              build=Path(self._tempdir.name, 'out'),
                              cwd=Path(self._tempdir.name, 'some', 'module'),
                              toolchain='//tools:fake_toolchain')

        module = Path(self._tempdir.name, 'out', 'fake_toolchain', 'obj',
                      'fake_module')
        os.makedirs(module)
        module.joinpath('fake_test.ninja').write_text(NINJA_EXECUTABLE)
        module.joinpath('fake_source_set.ninja').write_text(NINJA_SOURCE_SET)

        self._outdir = Path(self._tempdir.name, 'out', 'fake_toolchain', 'obj',
                            'fake_module')

    def tearDown(self):
        self._tempdir.cleanup()

    def test_source_set_artifact(self):
        target = TargetInfo(self._paths, '//fake_module:fake_source_set')
        self.assertIsNone(target.artifact)

    def test_source_set_object_files(self):
        target = TargetInfo(self._paths, '//fake_module:fake_source_set')
        self.assertEqual(
            set(target.object_files), {
                self._outdir / 'fake_source_set.file_a.cc.o',
                self._outdir / 'fake_source_set.file_b.c.o',
            })

    def test_executable_object_files(self):
        target = TargetInfo(self._paths, '//fake_module:fake_test')
        self.assertEqual(
            set(target.object_files), {
                self._outdir / 'fake_test.fake_test.cc.o',
                self._outdir / 'fake_test.fake_test_c.c.o',
            })

    def test_executable_artifact(self):
        target = TargetInfo(self._paths, '//fake_module:fake_test')
        self.assertEqual(target.artifact,
                         self._outdir / 'test' / 'fake_test.elf')


if __name__ == '__main__':
    unittest.main()
