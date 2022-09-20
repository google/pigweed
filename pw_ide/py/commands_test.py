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
"""Tests for pw_ide.commands"""

import os
import unittest

from pw_ide.commands import cmd_init
from pw_ide.cpp import CLANGD_WRAPPER_FILE_NAME
from pw_ide.python import PYTHON_BIN_DIR_SYMLINK_NAME, PYTHON_SYMLINK_NAME
from pw_ide.settings import PW_IDE_DIR_NAME

from test_cases import PwIdeTestCase


class TestCmdInit(PwIdeTestCase):
    """Tests cmd_init"""
    def test_make_dir_does_not_exist_creates_dir(self):
        settings = self.make_ide_settings(working_dir=PW_IDE_DIR_NAME)
        self.assertFalse(settings.working_dir.exists())
        cmd_init(make_dir=True,
                 make_clangd_wrapper=False,
                 make_python_symlink=False,
                 silent=True,
                 settings=settings)
        self.assertTrue(settings.working_dir.exists())

    def test_make_dir_does_exist_is_idempotent(self):
        settings = self.make_ide_settings(working_dir=PW_IDE_DIR_NAME)
        cmd_init(make_dir=True,
                 make_clangd_wrapper=False,
                 make_python_symlink=False,
                 silent=True,
                 settings=settings)
        modified_when_1 = os.path.getmtime(settings.working_dir)
        cmd_init(make_dir=True,
                 make_clangd_wrapper=False,
                 make_python_symlink=False,
                 silent=True,
                 settings=settings)
        modified_when_2 = os.path.getmtime(settings.working_dir)
        self.assertEqual(modified_when_1, modified_when_2)

    def test_make_clangd_wrapper_does_not_exist_creates_wrapper(self):
        settings = self.make_ide_settings()
        clangd_wrapper_path = settings.working_dir / CLANGD_WRAPPER_FILE_NAME
        self.assertFalse(clangd_wrapper_path.exists())
        cmd_init(make_dir=True,
                 make_clangd_wrapper=True,
                 make_python_symlink=False,
                 silent=True,
                 settings=settings)
        self.assertTrue(clangd_wrapper_path.exists())

    def test_make_clangd_wrapper_does_exist_is_idempotent(self):
        settings = self.make_ide_settings()
        clangd_wrapper_path = settings.working_dir / CLANGD_WRAPPER_FILE_NAME
        cmd_init(make_dir=True,
                 make_clangd_wrapper=True,
                 make_python_symlink=False,
                 silent=True,
                 settings=settings)
        modified_when_1 = os.path.getmtime(clangd_wrapper_path)
        cmd_init(make_dir=True,
                 make_clangd_wrapper=True,
                 make_python_symlink=False,
                 silent=True,
                 settings=settings)
        modified_when_2 = os.path.getmtime(clangd_wrapper_path)
        self.assertEqual(modified_when_1, modified_when_2)

    def test_make_python_symlink_does_not_exist_creates_symlink(self):
        settings = self.make_ide_settings()
        python_symlink_path = settings.working_dir / PYTHON_SYMLINK_NAME
        python_bin_dir_symlink_path = (settings.working_dir /
                                       PYTHON_BIN_DIR_SYMLINK_NAME)
        self.assertFalse(python_symlink_path.exists())
        self.assertFalse(python_bin_dir_symlink_path.exists())
        cmd_init(make_dir=True,
                 make_clangd_wrapper=False,
                 make_python_symlink=True,
                 silent=True,
                 settings=settings)
        self.assertTrue(python_symlink_path.exists())
        self.assertTrue(python_bin_dir_symlink_path.exists())

    def test_make_python_symlink_does_exist_is_idempotent(self):
        settings = self.make_ide_settings()
        python_symlink_path = settings.working_dir / PYTHON_SYMLINK_NAME
        python_bin_dir_symlink_path = (settings.working_dir /
                                       PYTHON_BIN_DIR_SYMLINK_NAME)
        cmd_init(make_dir=True,
                 make_clangd_wrapper=False,
                 make_python_symlink=True,
                 silent=True,
                 settings=settings)
        python_modified_when_1 = os.path.getmtime(python_symlink_path)
        python_bin_modified_when_1 = os.path.getmtime(
            python_bin_dir_symlink_path)
        cmd_init(make_dir=True,
                 make_clangd_wrapper=False,
                 make_python_symlink=True,
                 silent=True,
                 settings=settings)
        python_modified_when_2 = os.path.getmtime(python_symlink_path)
        python_bin_modified_when_2 = os.path.getmtime(
            python_bin_dir_symlink_path)
        self.assertEqual(python_modified_when_1, python_modified_when_2)
        self.assertEqual(python_bin_modified_when_1,
                         python_bin_modified_when_2)

    def test_do_everything(self):
        settings = self.make_ide_settings()
        cmd_init(make_dir=True,
                 make_clangd_wrapper=True,
                 make_python_symlink=True,
                 silent=True,
                 settings=settings)
        self.assertTrue(settings.working_dir.exists())
        self.assertTrue(
            (settings.working_dir / CLANGD_WRAPPER_FILE_NAME).exists())
        self.assertTrue(
            (settings.working_dir / CLANGD_WRAPPER_FILE_NAME).exists())
        self.assertTrue((settings.working_dir / PYTHON_SYMLINK_NAME).exists())
        self.assertTrue(
            (settings.working_dir / PYTHON_BIN_DIR_SYMLINK_NAME).exists())


if __name__ == '__main__':
    unittest.main()
