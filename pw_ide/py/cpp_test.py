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
"""Tests for pw_ide.cpp"""

import json
from pathlib import Path
from typing import List, Optional, Tuple, TypedDict, Union
import unittest
from unittest.mock import ANY, Mock, patch

# pylint: disable=protected-access
from pw_ide.cpp import (
    _COMPDB_FILE_PREFIX,
    _COMPDB_FILE_SEPARATOR,
    _COMPDB_FILE_EXTENSION,
    _COMPDB_CACHE_DIR_PREFIX,
    _COMPDB_CACHE_DIR_SEPARATOR,
    _target_and_executable_from_command,
    compdb_generate_cache_file_path,
    compdb_generate_file_path,
    compdb_target_from_path,
    CppCompilationDatabase,
    CppCompileCommand,
    CppCompileCommandDict,
    InvalidTargetException,
    MissingCompDbException,
    aggregate_compilation_database_targets,
    get_available_compdbs,
    get_available_targets,
    get_target,
    process_compilation_database,
    set_target,
)

from test_cases import PwIdeTestCase


class _TargetAndExecutableFromCommandTestCase(TypedDict):
    command: str
    target: Optional[str]
    executable: Optional[str]


class TestTargetAndExecutableFromCommand(unittest.TestCase):
    """Tests _target_and_executable_from_command"""
    def run_test(self, command: str, expected_target: Optional[str],
                 expected_executable: Optional[str]) -> None:
        (target, executable) = _target_and_executable_from_command(command)
        self.assertEqual(target, expected_target)
        self.assertEqual(executable, expected_executable)

    def test_correct_target_and_executable_with_gn_compile_command(
            self) -> None:
        """Test output against typical GN-generated compile commands."""

        cases: List[_TargetAndExecutableFromCommandTestCase] = [
            {
                # pylint: disable=line-too-long
                'command':
                'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'target': 'stm32f429i_disc1_debug',
                'executable': 'arm-none-eabi-g++',
            },
            {
                # pylint: disable=line-too-long
                'command':
                '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'target': 'pw_strict_host_clang_debug',
                'executable': 'clang++',
            },
            {
                # pylint: disable=line-too-long
                'command':
                "python ../pw_toolchain/py/pw_toolchain/clang_tidy.py  --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*'  --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/freelist.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_containers/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/freelist.cc -o  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o && touch  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o",
                # pylint: enable=line-too-long
                'target': None,
                'executable': 'python',
            },
            {
                'command': '',
                'target': None,
                'executable': None,
            },
        ]

        for case in cases:
            self.run_test(case['command'], case['target'], case['executable'])


class TestCppCompileCommand(unittest.TestCase):
    """Tests CppCompileCommand"""
    def test_post_init_frozen_attrs_set(self) -> None:
        command_dict = {
            # pylint: disable=line-too-long
            'command':
            'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
            # pylint: enable=line-too-long
            'directory': '/pigweed/pigweed/out',
            'file': '../pw_allocator/block.cc'
        }

        expected_target = 'stm32f429i_disc1_debug'
        expected_executable = 'arm-none-eabi-g++'
        command = CppCompileCommand(**command_dict)

        self.assertEqual(command.target, expected_target)
        self.assertEqual(command.executable, expected_executable)


class TestCppCompilationDatabase(PwIdeTestCase):
    """Tests CppCompilationDatabase"""
    def setUp(self):
        self.fixture: List[CppCompileCommandDict] = [
            {
                # pylint: disable=line-too-long
                'command':
                'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
            {
                # pylint: disable=line-too-long
                'command':
                '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
        ]

        return super().setUp()

    def test_load_from_dicts(self):
        compdb = CppCompilationDatabase.load(self.fixture)
        self.assertCountEqual(compdb.as_dicts(), self.fixture)

    def test_load_from_json(self):
        compdb = CppCompilationDatabase.load(json.dumps(self.fixture))
        self.assertCountEqual(compdb.as_dicts(), self.fixture)

    def test_load_from_path(self):
        with self.make_temp_file(
                f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_EXTENSION}',
                json.dumps(self.fixture)) as (_, file_path):
            path = file_path

        compdb = CppCompilationDatabase.load(path)
        self.assertCountEqual(compdb.as_dicts(), self.fixture)

    def test_load_from_file_handle(self):
        with self.make_temp_file(
                f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_EXTENSION}',
                json.dumps(self.fixture)) as (file, _):
            compdb = CppCompilationDatabase.load(file)

        self.assertCountEqual(compdb.as_dicts(), self.fixture)


class TestCompDbGenerateFilePath(unittest.TestCase):
    """Tests compdb_generate_file_path"""
    def test_with_target_includes_target(self) -> None:
        name = 'foo'
        actual = str(compdb_generate_file_path('foo'))
        expected = (f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
                    f'{name}{_COMPDB_FILE_EXTENSION}')
        self.assertEqual(actual, expected)

    def test_without_target_omits_target(self) -> None:
        actual = str(compdb_generate_file_path())
        expected = f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_EXTENSION}'
        self.assertEqual(actual, expected)


class TestCompDbGenerateCacheFilePath(unittest.TestCase):
    """Tests compdb_generate_cache_file_path"""
    def test_with_target_includes_target(self) -> None:
        name = 'foo'
        actual = str(compdb_generate_cache_file_path('foo'))
        expected = (f'{_COMPDB_CACHE_DIR_PREFIX}'
                    f'{_COMPDB_CACHE_DIR_SEPARATOR}{name}')
        self.assertEqual(actual, expected)

    def test_without_target_omits_target(self) -> None:
        actual = str(compdb_generate_cache_file_path())
        expected = f'{_COMPDB_CACHE_DIR_PREFIX}'
        self.assertEqual(actual, expected)


class _CompDbTargetFromPathTestCase(TypedDict):
    path: str
    target: Optional[str]


class TestCompDbTargetFromPath(unittest.TestCase):
    """Tests compdb_target_from_path"""
    def run_test(self, path: Path, expected_target: Optional[str]) -> None:
        target = compdb_target_from_path(path)
        self.assertEqual(target, expected_target)

    def test_correct_target_from_path(self) -> None:
        """Test that the expected target is extracted from the file path."""
        cases: List[_CompDbTargetFromPathTestCase] = [
            {
                'path':
                (f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
                 f'pw_strict_host_clang_debug{_COMPDB_FILE_EXTENSION}'),
                'target':
                'pw_strict_host_clang_debug'
            },
            {
                'path': (f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
                         f'stm32f429i_disc1_debug{_COMPDB_FILE_EXTENSION}'),
                'target':
                'stm32f429i_disc1_debug'
            },
            {
                'path': (f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
                         f'{_COMPDB_FILE_EXTENSION}'),
                'target':
                None
            },
            {
                'path': 'foompile_barmmands.json',
                'target': None
            },
            {
                'path': 'foompile_barmmands_target_x.json',
                'target': None
            },
            {
                'path': '',
                'target': None
            },
        ]

        for case in cases:
            self.run_test(Path(case['path']), case['target'])


class TestGetAvailableCompDbs(PwIdeTestCase):
    """Tests get_avaliable_compdbs"""
    def test_finds_all_compdbs(self) -> None:
        """Test that get_available_compdbs finds all compilation databases."""

        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
        ]

        # Simulate a dir with n compilation databases, m < n cache dirs, and
        # symlinks set up.
        files_data: List[Tuple[Union[Path, str], str]] = \
            [(compdb_generate_file_path(target), '') for target in targets]

        files_data.append((compdb_generate_cache_file_path(targets[0]), ''))
        files_data.append((compdb_generate_file_path(), ''))
        files_data.append((compdb_generate_cache_file_path(), ''))

        expected = [
            (f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
             f'{targets[0]}{_COMPDB_FILE_EXTENSION}',
             f'{_COMPDB_CACHE_DIR_PREFIX}{_COMPDB_CACHE_DIR_SEPARATOR}'
             f'{targets[0]}'),
            (f'{_COMPDB_FILE_PREFIX}{_COMPDB_FILE_SEPARATOR}'
             f'{targets[1]}{_COMPDB_FILE_EXTENSION}', None),
        ]

        settings = self.make_ide_settings(targets=targets)

        with self.make_temp_files(files_data):
            found_compdbs = get_available_compdbs(settings)

        # Strip out the temp dir path data.
        get_name = lambda p: p.name if p is not None else None
        found_compdbs_str = [(get_name(file), get_name(cache))
                             for file, cache in found_compdbs]

        self.assertCountEqual(found_compdbs_str, expected)


class TestGetAvailableTargets(PwIdeTestCase):
    """Tests get_available_targets"""
    def test_finds_all_targets(self) -> None:
        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
        ]

        settings = self.make_ide_settings(targets=targets)

        with self.make_temp_file(compdb_generate_file_path(targets[0])), \
             self.make_temp_file(compdb_generate_file_path(targets[1])):

            found_targets = get_available_targets(settings)

        self.assertCountEqual(found_targets, targets)


class TestGetTarget(PwIdeTestCase):
    """Tests get_target"""
    @patch('os.readlink')
    def test_finds_valid_target(self, mock_readlink: Mock) -> None:
        target = 'pw_strict_host_clang_debug'
        settings = self.make_ide_settings(targets=[target])
        mock_readlink.return_value = (f'{_COMPDB_FILE_PREFIX}'
                                      f'{_COMPDB_CACHE_DIR_SEPARATOR}'
                                      f'{target}{_COMPDB_FILE_EXTENSION}')
        found_target = get_target(settings)
        self.assertEqual(found_target, target)

    @patch('os.readlink')
    def test_finds_valid_target_nested(self, mock_readlink: Mock) -> None:
        target = 'pw_strict_host_clang_debug'
        settings = self.make_ide_settings(targets=[target])
        mock_readlink.return_value = (f'/x/y/z/{_COMPDB_FILE_PREFIX}'
                                      f'{_COMPDB_CACHE_DIR_SEPARATOR}'
                                      f'{target}{_COMPDB_FILE_EXTENSION}')
        found_target = get_target(settings)
        self.assertEqual(found_target, target)

    def test_returns_none_with_no_symlink(self) -> None:
        target = 'pw_strict_host_clang_debug'
        settings = self.make_ide_settings(targets=[target])
        found_target = get_target(settings)
        self.assertIsNone(found_target)


class TestSetTarget(PwIdeTestCase):
    """Tests set_target"""
    @patch('os.remove')
    @patch('os.mkdir')
    @patch('os.symlink')
    def test_sets_valid_target_when_no_target_set(self, mock_symlink: Mock,
                                                  mock_mkdir: Mock,
                                                  mock_remove: Mock) -> None:
        """Test the case where no symlinks have been set."""

        target = 'pw_strict_host_clang_debug'
        settings = self.make_ide_settings(targets=[target])
        compdb_symlink_path = compdb_generate_file_path()
        cache_symlink_path = compdb_generate_cache_file_path()

        with self.make_temp_file(compdb_generate_file_path(target)):
            set_target(target, settings)

            mock_mkdir.assert_any_call(
                self.path_in_temp_dir(compdb_generate_cache_file_path(target)))

            target_and_symlink_in_temp_dir = self.paths_in_temp_dir(
                compdb_generate_file_path(target), compdb_symlink_path)
            mock_symlink.assert_any_call(*target_and_symlink_in_temp_dir, ANY)

            target_and_symlink_in_temp_dir = self.paths_in_temp_dir(
                compdb_generate_cache_file_path(target), cache_symlink_path)
            mock_symlink.assert_any_call(*target_and_symlink_in_temp_dir, ANY)

            mock_remove.assert_not_called()

    @patch('os.remove')
    @patch('os.mkdir')
    @patch('os.symlink')
    def test_sets_valid_target_when_target_already_set(
            self, mock_symlink: Mock, mock_mkdir: Mock,
            mock_remove: Mock) -> None:
        """Test the case where symlinks have been set, and now we're setting
        them to a different target."""

        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
        ]

        settings = self.make_ide_settings(targets=targets)
        compdb_symlink_path = compdb_generate_file_path()
        cache_symlink_path = compdb_generate_cache_file_path()

        # Set the first target, which should initalize the symlinks.
        with self.make_temp_file(compdb_generate_file_path(targets[0])), \
             self.make_temp_file(compdb_generate_file_path(targets[1])):

            set_target(targets[0], settings)

            mock_mkdir.assert_any_call(
                self.path_in_temp_dir(
                    compdb_generate_cache_file_path(targets[0])))

            mock_remove.assert_not_called()

            # Simulate symlink creation
            with self.make_temp_file(compdb_symlink_path), \
                 self.make_temp_file(cache_symlink_path):

                # Set the second target, which should replace the symlinks
                set_target(targets[1], settings)

                mock_mkdir.assert_any_call(
                    self.path_in_temp_dir(
                        compdb_generate_cache_file_path(targets[1])))

                mock_remove.assert_any_call(
                    self.path_in_temp_dir(compdb_symlink_path))

                mock_remove.assert_any_call(
                    self.path_in_temp_dir(cache_symlink_path))

                target_and_symlink_in_temp_dir = self.paths_in_temp_dir(
                    compdb_generate_file_path(targets[1]), compdb_symlink_path)
                mock_symlink.assert_any_call(*target_and_symlink_in_temp_dir,
                                             ANY)

                target_and_symlink_in_temp_dir = self.paths_in_temp_dir(
                    compdb_generate_cache_file_path(targets[1]),
                    cache_symlink_path)
                mock_symlink.assert_any_call(*target_and_symlink_in_temp_dir,
                                             ANY)

    @patch('os.remove')
    @patch('os.mkdir')
    @patch('os.symlink')
    def test_sets_valid_target_back_and_forth(self, mock_symlink: Mock,
                                              mock_mkdir: Mock,
                                              mock_remove: Mock) -> None:
        """Test the case where symlinks have been set, we set them to a second
        target, and now we're setting them back to the first target."""

        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
        ]

        settings = self.make_ide_settings(targets=targets)
        compdb_symlink_path = compdb_generate_file_path()
        cache_symlink_path = compdb_generate_cache_file_path()

        # Set the first target, which should initalize the symlinks
        with self.make_temp_file(compdb_generate_file_path(targets[0])), \
             self.make_temp_file(compdb_generate_file_path(targets[1])):

            set_target(targets[0], settings)

            # Simulate symlink creation
            with self.make_temp_file(compdb_symlink_path), \
                 self.make_temp_file(cache_symlink_path):

                # Set the second target, which should replace the symlinks
                set_target(targets[1], settings)

                # Reset mocks to clear events prior to those under test
                mock_symlink.reset_mock()
                mock_mkdir.reset_mock()
                mock_remove.reset_mock()

                # Set the first target again, which should also replace the
                # symlinks and reuse the existing cache folder
                set_target(targets[0], settings)

                mock_mkdir.assert_any_call(
                    self.path_in_temp_dir(
                        compdb_generate_cache_file_path(targets[0])))

                mock_remove.assert_any_call(
                    self.path_in_temp_dir(compdb_symlink_path))

                mock_remove.assert_any_call(
                    self.path_in_temp_dir(cache_symlink_path))

                target_and_symlink_in_temp_dir = self.paths_in_temp_dir(
                    compdb_generate_file_path(targets[0]), compdb_symlink_path)
                mock_symlink.assert_any_call(*target_and_symlink_in_temp_dir,
                                             ANY)

                target_and_symlink_in_temp_dir = self.paths_in_temp_dir(
                    compdb_generate_cache_file_path(targets[0]),
                    cache_symlink_path)
                mock_symlink.assert_any_call(*target_and_symlink_in_temp_dir,
                                             ANY)

    @patch('os.symlink')
    def test_invalid_target_not_in_defined_targets_raises(
            self, mock_symlink: Mock):
        target = 'pw_strict_host_clang_debug'
        settings = self.make_ide_settings(targets=[target])

        with self.make_temp_file(compdb_generate_file_path(target)), \
             self.assertRaises(InvalidTargetException):

            set_target('foo', settings)
            mock_symlink.assert_not_called()

    @patch('os.symlink')
    def test_invalid_target_not_in_available_targets_raises(
            self, mock_symlink: Mock):
        target = 'pw_strict_host_clang_debug'
        settings = self.make_ide_settings(targets=[target])

        with self.assertRaises(MissingCompDbException):
            set_target(target, settings)
            mock_symlink.assert_not_called()


class TestAggregateCompilationDatabaseTargets(PwIdeTestCase):
    """Tests aggregate_compilation_database_targets"""
    def test_gets_all_legitimate_targets(self):
        """Test compilation target aggregation against a typical sample of raw
        output from GN."""

        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
        ]

        raw_db: List[CppCompileCommandDict] = [
            {
                # pylint: disable=line-too-long
                'command':
                'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
            {
                # pylint: disable=line-too-long
                'command':
                '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
            {
                # pylint: disable=line-too-long
                'command':
                "python ../pw_toolchain/py/pw_toolchain/clang_tidy.py  --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*'  --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/block.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o && touch  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o",
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc',
            },
        ]

        aggregated_targets = aggregate_compilation_database_targets(raw_db)
        self.assertCountEqual(aggregated_targets, targets)


class TestProcessCompilationDatabase(PwIdeTestCase):
    """Tests process_compilation_database"""
    def test_compilation_database_processed_correctly(self):
        """Test compilation database processing against a typical sample of
        raw output from GN."""

        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
            'isosceles_debug',
        ]

        settings = self.make_ide_settings(targets=targets)

        raw_db: List[CppCompileCommandDict] = [
            {
                # pylint: disable=line-too-long
                'command':
                'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
            {
                # pylint: disable=line-too-long
                'command':
                '../environment/cipd/packages/pigweed/bin/isosceles-clang++ -MMD -MF  isosceles_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
            {
                # pylint: disable=line-too-long
                'command':
                '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc'
            },
            {
                # pylint: disable=line-too-long
                'command':
                "python ../pw_toolchain/py/pw_toolchain/clang_tidy.py  --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*'  --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/block.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o && touch  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o",
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc',
            },
        ]

        expected_compdbs = {
            'isosceles_debug': [
                {
                    # pylint: disable=line-too-long
                    'command':
                    '../environment/cipd/packages/pigweed/bin/isosceles-clang++ -MMD -MF  isosceles_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                    # pylint: enable=line-too-long
                    'directory': '/pigweed/pigweed/out',
                    'file': '../pw_allocator/block.cc'
                },
            ],
            'pw_strict_host_clang_debug': [
                {
                    # pylint: disable=line-too-long
                    'command':
                    '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                    # pylint: enable=line-too-long
                    'directory': '/pigweed/pigweed/out',
                    'file': '../pw_allocator/block.cc'
                },
            ],
            'stm32f429i_disc1_debug': [
                {
                    # pylint: disable=line-too-long
                    'command':
                    'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                    # pylint: enable=line-too-long
                    'directory': '/pigweed/pigweed/out',
                    'file': '../pw_allocator/block.cc'
                },
            ],
        }

        compdbs = process_compilation_database(raw_db, settings)
        compdbs_as_dicts = {
            target: compdb.as_dicts()
            for target, compdb in compdbs.items()
        }
        self.assertDictEqual(compdbs_as_dicts, expected_compdbs)


if __name__ == '__main__':
    unittest.main()
