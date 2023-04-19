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
from typing import cast, Dict, List, Optional
import unittest

# pylint: disable=protected-access
from pw_ide.cpp import (
    COMPDB_FILE_NAME,
    command_parts,
    path_to_executable,
    CppCompilationDatabase,
    CppCompilationDatabasesMap,
    CppCompileCommand,
    CppCompileCommandDict,
    infer_target,
    _infer_target_pos,
)

from pw_ide.exceptions import UnresolvablePathException

from test_cases import PwIdeTestCase


class TestPathToExecutable(PwIdeTestCase):
    """Tests path_to_executable"""

    def test_valid_absolute_path_in_env_returns_same_path(self):
        executable_path = self.temp_dir_path / 'clang'
        query_drivers = [f'{self.temp_dir_path}/*']
        result = path_to_executable(
            str(executable_path.absolute()), path_globs=query_drivers
        )
        self.assertIsNotNone(result)
        self.assertEqual(str(result), str(executable_path.absolute()))

    def test_valid_absolute_path_outside_env_returns_same_path(self):
        executable_path = Path('/usr/bin/clang')
        query_drivers = ['/**/*']
        result = path_to_executable(
            str(executable_path.absolute()), path_globs=query_drivers
        )
        self.assertIsNotNone(result)
        self.assertEqual(str(result), str(executable_path.absolute()))

    def test_valid_relative_path_returns_same_path(self):
        executable_path = Path('../clang')
        query_drivers = [f'{self.temp_dir_path}/*']
        result = path_to_executable(
            str(executable_path), path_globs=query_drivers
        )
        self.assertIsNotNone(result)
        self.assertEqual(str(result), str(executable_path))

    def test_valid_no_path_returns_new_path(self):
        executable_path = Path('clang')
        query_drivers = [f'{self.temp_dir_path}/*']
        expected_path = self.temp_dir_path / executable_path
        self.touch_temp_file(expected_path)
        result = path_to_executable(
            str(executable_path), path_globs=query_drivers
        )
        self.assertIsNotNone(result)
        self.assertEqual(str(result), str(expected_path))

    def test_invalid_absolute_path_in_env_returns_same_path(self):
        executable_path = self.temp_dir_path / '_pw_invalid_exe'
        query_drivers = [f'{self.temp_dir_path}/*']
        result = path_to_executable(
            str(executable_path.absolute()), path_globs=query_drivers
        )
        self.assertIsNone(result)

    def test_invalid_absolute_path_outside_env_returns_same_path(self):
        executable_path = Path('/usr/bin/_pw_invalid_exe')
        query_drivers = ['/**/*']
        result = path_to_executable(
            str(executable_path.absolute()), path_globs=query_drivers
        )
        self.assertIsNone(result)

    def test_invalid_relative_path_returns_same_path(self):
        executable_path = Path('../_pw_invalid_exe')
        query_drivers = [f'{self.temp_dir_path}/*']
        result = path_to_executable(
            str(executable_path), path_globs=query_drivers
        )
        self.assertIsNone(result)

    def test_invalid_no_path_returns_new_path(self):
        executable_path = Path('_pw_invalid_exe')
        query_drivers = [f'{self.temp_dir_path}/*']
        expected_path = self.temp_dir_path / executable_path
        self.touch_temp_file(expected_path)
        result = path_to_executable(
            str(executable_path), path_globs=query_drivers
        )
        self.assertIsNone(result)


class TestInferTarget(unittest.TestCase):
    """Tests infer_target"""

    def test_infer_target_pos(self):
        test_cases = [
            ('?', [0]),
            ('*/?', [1]),
            ('*/*/?', [2]),
            ('*/?/?', [1, 2]),
        ]

        for glob, result in test_cases:
            self.assertEqual(_infer_target_pos(glob), result)

    def test_infer_target(self):
        test_cases = [
            ('?', 'target/thing.o', 'target'),
            ('*/?', 'variants/target/foo/bar/thing.o', 'target'),
            ('*/*/*/*/?', 'foo/bar/baz/hi/target/obj/thing.o', 'target'),
            ('*/?/?', 'variants/target/foo/bar/thing.o', 'target_foo'),
        ]

        for glob, output_path, result in test_cases:
            self.assertEqual(
                infer_target(glob, Path(''), Path(output_path)), result
            )


class TestCppCompileCommand(PwIdeTestCase):
    """Tests CppCompileCommand"""

    def run_process_test_with_valid_commands(
        self, command: str, expected_executable: Optional[str]
    ) -> None:
        compile_command = CppCompileCommand(
            command=command, file='', directory=''
        )
        processed_compile_command = cast(
            CppCompileCommand,
            compile_command.process(default_path=self.temp_dir_path),
        )
        self.assertIsNotNone(processed_compile_command)
        self.assertEqual(
            processed_compile_command.executable_name, expected_executable
        )

    def run_process_test_with_invalid_commands(self, command: str) -> None:
        compile_command = CppCompileCommand(
            command=command, file='', directory=''
        )

        with self.assertRaises(UnresolvablePathException):
            compile_command.process(default_path=self.temp_dir_path)

    def test_process_valid_with_gn_compile_command(self) -> None:
        """Test output against typical GN-generated compile commands."""

        cases: List[Dict[str, str]] = [
            {
                # pylint: disable=line-too-long
                'command': 'arm-none-eabi-g++ -MMD -MF  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'executable': 'arm-none-eabi-g++',
            },
            {
                # pylint: disable=line-too-long
                'command': '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'executable': 'clang++',
            },
        ]

        for case in cases:
            self.run_process_test_with_valid_commands(
                case['command'], case['executable']
            )

    def test_process_invalid_with_gn_compile_command(self) -> None:
        """Test output against typical GN-generated compile commands."""

        # pylint: disable=line-too-long
        command = "python ../pw_toolchain/py/pw_toolchain/clang_tidy.py  --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*'  --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/freelist.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_containers/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/freelist.cc -o  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o && touch  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/freelist.freelist.cc.o"
        # pylint: enable=line-too-long

        compile_command = CppCompileCommand(
            command=command, file='', directory=''
        )
        self.assertIsNone(
            compile_command.process(default_path=self.temp_dir_path)
        )

    def test_process_unresolvable_with_gn_compile_command(self) -> None:
        """Test output against typical GN-generated compile commands."""

        # pylint: disable=line-too-long
        command = 'clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o'
        # pylint: enable=line-too-long

        compile_command = CppCompileCommand(
            command=command, file='', directory=''
        )

        processed_compile_command = cast(
            CppCompileCommand, compile_command.process()
        )
        self.assertIsNotNone(processed_compile_command)
        self.assertIsNotNone(processed_compile_command.command)
        # .split() to avoid failing on whitespace differences.
        self.assertCountEqual(
            cast(str, processed_compile_command.command).split(),
            command.split(),
        )

    def test_process_unresolvable_strict_with_gn_compile_command(self) -> None:
        """Test output against typical GN-generated compile commands."""

        # pylint: disable=line-too-long
        command = 'clang++ -MMD -MF  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d  -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o'
        # pylint: enable=line-too-long

        compile_command = CppCompileCommand(
            command=command, file='', directory=''
        )

        with self.assertRaises(UnresolvablePathException):
            compile_command.process(strict=True)

    def test_process_blank_with_gn_compile_command(self) -> None:
        """Test output against typical GN-generated compile commands."""

        command = ''
        compile_command = CppCompileCommand(
            command=command, file='', directory=''
        )

        result = compile_command.process(default_path=self.temp_dir_path)
        self.assertIsNone(result)


class TestCommandParts(unittest.TestCase):
    """Test command_parts"""

    def test_command_parts(self):
        """Test command_parts"""

        # pylint: disable=line-too-long
        test_cases = [
            "python ../pw_toolchain/py/pw_toolchain/clang_tidy.py --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*' --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/block.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o && touch pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o",
            'arm-none-eabi-g++ -MMD -MF stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
            '../environment/cipd/packages/pigweed/bin/isosceles-clang++ -MMD -MF isosceles_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o isosceles_debug/obj/pw_allocator/block.block.cc.o',
            '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
            'ccache arm-none-eabi-g++ -MMD -MF stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
            'ccache ../environment/cipd/packages/pigweed/bin/isosceles-clang++ -MMD -MF isosceles_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o isosceles_debug/obj/pw_allocator/block.block.cc.o',
            'ccache ../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
            'ccache debug=true max_size=10G arm-none-eabi-g++ -MMD -MF stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
            'ccache debug=true max_size=10G ../environment/cipd/packages/pigweed/bin/isosceles-clang++ -MMD -MF isosceles_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o isosceles_debug/obj/pw_allocator/block.block.cc.o',
            'ccache debug=true max_size=10G ../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
            "ccache python ../pw_toolchain/py/pw_toolchain/clang_tidy.py --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*' --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/block.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o && touch pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o",
        ]

        expected_results = [
            (
                None,
                'python',
                [
                    '../pw_toolchain/py/pw_toolchain/clang_tidy.py',
                    '--source-exclude',
                    "'third_party/.*'",
                    '--source-exclude',
                    "'.*packages/mbedtls.*'",
                    '--source-exclude',
                    "'.*packages/boringssl.*'",
                    '--skip-include-path',
                    "'mbedtls/include'",
                    '--skip-include-path',
                    "'mbedtls'",
                    '--skip-include-path',
                    "'boringssl/src/include'",
                    '--skip-include-path',
                    "'boringssl'",
                    '--skip-include-path',
                    "'pw_tls_client/generate_test_data'",
                    '--source-file',
                    '../pw_allocator/block.cc',
                    '--source-root',
                    "'../'",
                    '--export-fixes',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml',
                    '--',
                    '../environment/cipd/packages/pigweed/bin/clang++',
                    'END_OF_INVOKER',
                    '-MMD',
                    '-MF',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o',
                    '&&',
                    'touch',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                None,
                'arm-none-eabi-g++',
                [
                    '-MMD',
                    '-MF',
                    'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-Wno-psabi',
                    '-mabi=aapcs',
                    '-mthumb',
                    '--sysroot=../environment/cipd/packages/arm',
                    '-specs=nano.specs',
                    '-specs=nosys.specs',
                    '-mcpu=cortex-m4',
                    '-mfloat-abi=hard',
                    '-mfpu=fpv4-sp-d16',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-u_printf_float',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-DPW_ARMV7M_ENABLE_FPU=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/assert_compatibility_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                None,
                '../environment/cipd/packages/pigweed/bin/isosceles-clang++',
                [
                    '-MMD',
                    '-MF',
                    'isosceles_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'isosceles_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                None,
                '../environment/cipd/packages/pigweed/bin/clang++',
                [
                    '-MMD',
                    '-MF',
                    'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache',
                'arm-none-eabi-g++',
                [
                    '-MMD',
                    '-MF',
                    'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-Wno-psabi',
                    '-mabi=aapcs',
                    '-mthumb',
                    '--sysroot=../environment/cipd/packages/arm',
                    '-specs=nano.specs',
                    '-specs=nosys.specs',
                    '-mcpu=cortex-m4',
                    '-mfloat-abi=hard',
                    '-mfpu=fpv4-sp-d16',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-u_printf_float',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-DPW_ARMV7M_ENABLE_FPU=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/assert_compatibility_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache',
                '../environment/cipd/packages/pigweed/bin/isosceles-clang++',
                [
                    '-MMD',
                    '-MF',
                    'isosceles_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'isosceles_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache',
                '../environment/cipd/packages/pigweed/bin/clang++',
                [
                    '-MMD',
                    '-MF',
                    'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache debug=true max_size=10G',
                'arm-none-eabi-g++',
                [
                    '-MMD',
                    '-MF',
                    'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-Wno-psabi',
                    '-mabi=aapcs',
                    '-mthumb',
                    '--sysroot=../environment/cipd/packages/arm',
                    '-specs=nano.specs',
                    '-specs=nosys.specs',
                    '-mcpu=cortex-m4',
                    '-mfloat-abi=hard',
                    '-mfpu=fpv4-sp-d16',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-u_printf_float',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-DPW_ARMV7M_ENABLE_FPU=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/assert_compatibility_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache debug=true max_size=10G',
                '../environment/cipd/packages/pigweed/bin/isosceles-clang++',
                [
                    '-MMD',
                    '-MF',
                    'isosceles_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'isosceles_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache debug=true max_size=10G',
                '../environment/cipd/packages/pigweed/bin/clang++',
                [
                    '-MMD',
                    '-MF',
                    'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                ],
            ),
            (
                'ccache',
                'python',
                [
                    '../pw_toolchain/py/pw_toolchain/clang_tidy.py',
                    '--source-exclude',
                    "'third_party/.*'",
                    '--source-exclude',
                    "'.*packages/mbedtls.*'",
                    '--source-exclude',
                    "'.*packages/boringssl.*'",
                    '--skip-include-path',
                    "'mbedtls/include'",
                    '--skip-include-path',
                    "'mbedtls'",
                    '--skip-include-path',
                    "'boringssl/src/include'",
                    '--skip-include-path',
                    "'boringssl'",
                    '--skip-include-path',
                    "'pw_tls_client/generate_test_data'",
                    '--source-file',
                    '../pw_allocator/block.cc',
                    '--source-root',
                    "'../'",
                    '--export-fixes',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml',
                    '--',
                    '../environment/cipd/packages/pigweed/bin/clang++',
                    'END_OF_INVOKER',
                    '-MMD',
                    '-MF',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d',
                    '-g3',
                    '--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
                    '-Og',
                    '-Wshadow',
                    '-Wredundant-decls',
                    '-Wthread-safety',
                    '-Wswitch-enum',
                    '-fdiagnostics-color',
                    '-g',
                    '-fno-common',
                    '-fno-exceptions',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-Wall',
                    '-Wextra',
                    '-Wimplicit-fallthrough',
                    '-Wcast-qual',
                    '-Wundef',
                    '-Wpointer-arith',
                    '-Werror',
                    '-Wno-error=cpp',
                    '-Wno-error=deprecated-declarations',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-ffile-prefix-map=/pigweed/pigweed/=',
                    '-ffile-prefix-map=../=',
                    '-ffile-prefix-map=/pigweed/pigweed/out=out',
                    '-Wextra-semi',
                    '-fno-rtti',
                    '-Wnon-virtual-dtor',
                    '-std=c++17',
                    '-Wno-register',
                    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
                    '-DPW_STATUS_CFG_CHECK_IF_USED=1',
                    '-I../pw_allocator/public',
                    '-I../pw_assert/public',
                    '-I../pw_assert/print_and_abort_assert_public_overrides',
                    '-I../pw_preprocessor/public',
                    '-I../pw_assert_basic/public_overrides',
                    '-I../pw_assert_basic/public',
                    '-I../pw_span/public',
                    '-I../pw_polyfill/public',
                    '-I../pw_polyfill/standard_library_public',
                    '-I../pw_status/public',
                    '-c',
                    '../pw_allocator/block.cc',
                    '-o',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o',
                    '&&',
                    'touch',
                    'pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o',
                ],
            ),
        ]
        # pylint: enable=line-too-long

        for test_case, expected in zip(test_cases, expected_results):
            self.assertEqual(command_parts(test_case), expected)


class TestCppCompilationDatabase(PwIdeTestCase):
    """Tests CppCompilationDatabase"""

    def setUp(self):
        self.build_dir = Path('/pigweed/pigweed/out')

        self.fixture: List[CppCompileCommandDict] = [
            {
                # pylint: disable=line-too-long
                'command': 'arm-none-eabi-g++ -MMD -MF stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -DPW_ARMV7M_ENABLE_FPU=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc',
            },
            {
                # pylint: disable=line-too-long
                'command': '../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc',
            },
        ]

        self.expected: List[CppCompileCommandDict] = [
            {
                **self.fixture[0],
                # pylint: disable=line-too-long
                'output': 'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
            },
            {
                **self.fixture[1],
                # pylint: disable=line-too-long
                'output': 'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
            },
        ]

        self.fixture_merge_1 = [
            {
                # pylint: disable=line-too-long
                'command': 'g++ -MMD -MF  pw_strict_host_gcc_debug/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_gcc_debug/obj/pw_allocator/block.block.cc.o',
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/block.cc',
                'output': 'pw_strict_host_gcc_debug/obj/pw_allocator/block.block.cc.o',
                # pylint: enable=line-too-long
            },
            {
                # pylint: disable=line-too-long
                'command': 'g++ -MMD -MF  pw_strict_host_gcc_debug/obj/pw_allocator/freelist.freelist.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=//pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_containers/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_span/public -I../pw_status/public -c ../pw_allocator/freelist.cc -o  pw_strict_host_gcc_debug/obj/pw_allocator/freelist.freelist.cc.o',
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_allocator/freelist.cc',
                'output': 'pw_strict_host_gcc_debug/obj/pw_allocator/freelist.freelist.cc.o',
                # pylint: enable=line-too-long
            },
        ]

        self.fixture_merge_2 = [
            {
                # pylint: disable=line-too-long
                'command': 'g++ -MMD -MF  pw_strict_host_gcc_debug/obj/pw_base64/pw_base64.base64.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true  -I../pw_base64/public -I../pw_string/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_span/public -c ../pw_base64/base64.cc -o  pw_strict_host_gcc_debug/obj/pw_base64/pw_base64.base64.cc.o',
                'directory': '/pigweed/pigweed/out',
                'file': '../pw_base64/base64.cc',
                'output': 'pw_strict_host_gcc_debug/obj/pw_base64/pw_base64.base64.cc.o',
                # pylint: enable=line-too-long
            },
            {
                # pylint: disable=line-too-long
                'command': 'g++ -MMD -MF  pw_strict_host_gcc_debug/obj/pw_checksum/pw_checksum.crc32.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_checksum/public -I../pw_bytes/public -I../pw_containers/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_preprocessor/public -I../pw_span/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_status/public -c ../pw_checksum/crc32.cc -o  pw_strict_host_gcc_debug/obj/pw_checksum/pw_checksum.crc32.cc.o',
                'directory': 'pigweed/pigweed/out',
                'file': '../pw_checksum/crc32.cc',
                'output': 'pw_strict_host_gcc_debug/obj/pw_checksum/pw_checksum.crc32.cc.o',
                # pylint: enable=line-too-long
            },
        ]

        return super().setUp()

    def test_merge(self):
        compdb1 = CppCompilationDatabase.load(
            self.fixture_merge_1, self.build_dir
        )
        compdb2 = CppCompilationDatabase.load(
            self.fixture_merge_2, self.build_dir
        )
        compdb1.merge(compdb2)
        result = [compile_command.as_dict() for compile_command in compdb1]
        expected = [*self.fixture_merge_1, *self.fixture_merge_2]
        self.assertCountEqual(result, expected)

    def test_merge_no_dupes(self):
        compdb1 = CppCompilationDatabase.load(
            self.fixture_merge_1, self.build_dir
        )
        fixture_combo = [*self.fixture_merge_1, *self.fixture_merge_2]
        compdb2 = CppCompilationDatabase.load(fixture_combo, self.build_dir)
        compdb1.merge(compdb2)
        result = [compile_command.as_dict() for compile_command in compdb1]
        expected = [*self.fixture_merge_1, *self.fixture_merge_2]
        self.assertCountEqual(result, expected)

    def test_load_from_dicts(self):
        compdb = CppCompilationDatabase.load(self.fixture, self.build_dir)
        self.assertCountEqual(compdb.as_dicts(), self.expected)

    def test_load_from_json(self):
        compdb = CppCompilationDatabase.load(
            json.dumps(self.fixture), self.build_dir
        )
        self.assertCountEqual(compdb.as_dicts(), self.expected)

    def test_load_from_path(self):
        with self.make_temp_file(
            COMPDB_FILE_NAME,
            json.dumps(self.fixture),
        ) as (_, file_path):
            path = file_path

        compdb = CppCompilationDatabase.load(path, self.build_dir)
        self.assertCountEqual(compdb.as_dicts(), self.expected)

    def test_load_from_file_handle(self):
        with self.make_temp_file(
            COMPDB_FILE_NAME,
            json.dumps(self.fixture),
        ) as (file, _):
            compdb = CppCompilationDatabase.load(file, self.build_dir)

        self.assertCountEqual(compdb.as_dicts(), self.expected)

    def test_process(self):
        """Test processing against a typical sample of raw output from GN."""

        targets = [
            'pw_strict_host_clang_debug',
            'stm32f429i_disc1_debug',
            'isosceles_debug',
        ]

        settings = self.make_ide_settings(targets=targets)

        # pylint: disable=line-too-long
        raw_db: List[CppCompileCommandDict] = [
            {
                'command': 'arm-none-eabi-g++ -MMD -MF stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -DPW_ARMV7M_ENABLE_FPU=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                'directory': str(self.build_dir),
                'file': '../pw_allocator/block.cc',
            },
            {
                'command': '../environment/cipd/packages/pigweed/bin/isosceles-clang++ -MMD -MF isosceles_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o isosceles_debug/obj/pw_allocator/block.block.cc.o',
                'directory': str(self.build_dir),
                'file': '../pw_allocator/block.cc',
            },
            {
                'command': 'ccache ../environment/cipd/packages/pigweed/bin/clang++ -MMD -MF pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                'directory': str(self.build_dir),
                'file': '../pw_allocator/block.cc',
            },
            {
                'command': "python ../pw_toolchain/py/pw_toolchain/clang_tidy.py --source-exclude 'third_party/.*' --source-exclude '.*packages/mbedtls.*' --source-exclude '.*packages/boringssl.*' --skip-include-path 'mbedtls/include' --skip-include-path 'mbedtls' --skip-include-path 'boringssl/src/include' --skip-include-path 'boringssl' --skip-include-path 'pw_tls_client/generate_test_data' --source-file ../pw_allocator/block.cc --source-root '../' --export-fixes  pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.yaml -- ../environment/cipd/packages/pigweed/bin/clang++ END_OF_INVOKER -MMD -MF pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o && touch pw_strict_host_clang_debug.static_analysis/obj/pw_allocator/block.block.cc.o",
                'directory': str(self.build_dir),
                'file': '../pw_allocator/block.cc',
            },
        ]

        expected_compdbs = {
            'isosceles_debug': [
                {
                    'command':
                    # Ensures path format matches OS (e.g. Windows)
                    f'{Path("../environment/cipd/packages/pigweed/bin/isosceles-clang++")} -MMD -MF isosceles_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o isosceles_debug/obj/pw_allocator/block.block.cc.o',
                    'directory': str(self.build_dir),
                    'file': '../pw_allocator/block.cc',
                    'output': 'isosceles_debug/obj/pw_allocator/block.block.cc.o',
                },
            ],
            'pw_strict_host_clang_debug': [
                {
                    'command':
                    # Ensures path format matches OS (e.g. Windows)
                    f'ccache {Path("../environment/cipd/packages/pigweed/bin/clang++")} -MMD -MF pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o.d -g3 --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -Og -Wshadow -Wredundant-decls -Wthread-safety -Wswitch-enum -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1 -DPW_STATUS_CFG_CHECK_IF_USED=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                    'directory': str(self.build_dir),
                    'file': '../pw_allocator/block.cc',
                    'output': 'pw_strict_host_clang_debug/obj/pw_allocator/block.block.cc.o',
                },
            ],
            'stm32f429i_disc1_debug': [
                {
                    'command':
                    # Ensures this test avoids the unpathed compiler search
                    f'{self.temp_dir_path / "arm-none-eabi-g++"} -MMD -MF stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o.d -Wno-psabi -mabi=aapcs -mthumb --sysroot=../environment/cipd/packages/arm -specs=nano.specs -specs=nosys.specs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Og -Wshadow -Wredundant-decls -u_printf_float -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register -DPW_ARMV7M_ENABLE_FPU=1 -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/assert_compatibility_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                    'directory': str(self.build_dir),
                    'file': '../pw_allocator/block.cc',
                    'output': 'stm32f429i_disc1_debug/obj/pw_allocator/block.block.cc.o',
                },
            ],
        }
        # pylint: enable=line-too-long

        compdbs = CppCompilationDatabase.load(
            raw_db, build_dir=self.build_dir
        ).process(settings, default_path=self.temp_dir_path)
        compdbs_as_dicts = {
            target: compdb.as_dicts() for target, compdb in compdbs.items()
        }

        self.assertCountEqual(
            list(compdbs_as_dicts.keys()), list(expected_compdbs.keys())
        )
        self.assertDictEqual(compdbs_as_dicts, expected_compdbs)


class TestCppCompilationDatabasesMap(PwIdeTestCase):
    """Tests CppCompilationDatabasesMap"""

    def setUp(self):
        # pylint: disable=line-too-long
        self.fixture_1 = lambda target: [
            CppCompileCommand(
                **{
                    'command': f'g++ -MMD -MF  {target}/obj/pw_allocator/block.block.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_span/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_status/public -c ../pw_allocator/block.cc -o  pw_strict_host_gcc_debug/obj/pw_allocator/block.block.cc.o',
                    'directory': '/pigweed/pigweed/out',
                    'file': '../pw_allocator/block.cc',
                }
            ),
            CppCompileCommand(
                **{
                    'command': f'g++ -MMD -MF  {target}/obj/pw_allocator/freelist.freelist.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=//pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_allocator/public -I../pw_containers/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_span/public -I../pw_status/public -c ../pw_allocator/freelist.cc -o  pw_strict_host_gcc_debug/obj/pw_allocator/freelist.freelist.cc.o',
                    'directory': '/pigweed/pigweed/out',
                    'file': '../pw_allocator/freelist.cc',
                }
            ),
        ]

        self.fixture_2 = lambda target: [
            CppCompileCommand(
                **{
                    'command': f'g++ -MMD -MF  {target}/obj/pw_base64/pw_base64.base64.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true  -I../pw_base64/public -I../pw_string/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_preprocessor/public -I../pw_assert_basic/public_overrides -I../pw_assert_basic/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_span/public -c ../pw_base64/base64.cc -o  pw_strict_host_gcc_debug/obj/pw_base64/pw_base64.base64.cc.o',
                    'directory': '/pigweed/pigweed/out',
                    'file': '../pw_base64/base64.cc',
                }
            ),
            CppCompileCommand(
                **{
                    'command': f'g++ -MMD -MF  {target}/obj/pw_checksum/pw_checksum.crc32.cc.o.d  -Wno-psabi -Og -Wshadow -Wredundant-decls -Wswitch-enum -Wpedantic -Wno-c++20-designator -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Wimplicit-fallthrough -Wcast-qual -Wundef -Wpointer-arith -Werror -Wno-error=cpp -Wno-error=deprecated-declarations -ffile-prefix-map=/pigweed/pigweed/out=out -ffile-prefix-map=/pigweed/pigweed/= -ffile-prefix-map=../= -ffile-prefix-map=/pigweed/pigweed/out=out  -Wextra-semi -fno-rtti -Wnon-virtual-dtor -std=c++17 -Wno-register  -DPW_SPAN_ENABLE_ASSERTS=true -DPW_STATUS_CFG_CHECK_IF_USED=1  -I../pw_checksum/public -I../pw_bytes/public -I../pw_containers/public -I../pw_polyfill/public -I../pw_polyfill/standard_library_public -I../pw_preprocessor/public -I../pw_span/public -I../pw_assert/public -I../pw_assert/print_and_abort_assert_public_overrides -I../pw_status/public -c ../pw_checksum/crc32.cc -o  pw_strict_host_gcc_debug/obj/pw_checksum/pw_checksum.crc32.cc.o',
                    'directory': 'pigweed/pigweed/out',
                    'file': '../pw_checksum/crc32.cc',
                }
            ),
        ]
        # pylint: enable=line-too-long
        super().setUp()

    def test_merge_0_db_set(self):
        with self.assertRaises(ValueError):
            CppCompilationDatabasesMap.merge()

    def test_merge_1_db_set(self):
        settings = self.make_ide_settings()
        target = 'test_target'
        db_set = CppCompilationDatabasesMap(settings)
        db_set[target] = self.fixture_1(target)
        result = CppCompilationDatabasesMap.merge(db_set)
        self.assertCountEqual(result._dbs, db_set._dbs)

    def test_merge_2_db_sets_different_targets(self):
        settings = self.make_ide_settings()
        target1 = 'test_target_1'
        target2 = 'test_target_2'
        db_set1 = CppCompilationDatabasesMap(settings)
        db_set1[target1] = self.fixture_1(target1)
        db_set2 = CppCompilationDatabasesMap(settings)
        db_set2[target2] = self.fixture_2(target2)
        result = CppCompilationDatabasesMap.merge(db_set1, db_set2)
        self.assertEqual(len(result), 2)
        self.assertCountEqual(result._dbs, {**db_set1._dbs, **db_set2._dbs})

    def test_merge_2_db_sets_duplicated_targets(self):
        settings = self.make_ide_settings()
        target1 = 'test_target_1'
        target2 = 'test_target_2'
        db_set1 = CppCompilationDatabasesMap(settings)
        db_set1[target1] = self.fixture_1(target1)
        db_set2 = CppCompilationDatabasesMap(settings)
        db_set2[target2] = self.fixture_2(target2)
        db_set_combo = CppCompilationDatabasesMap.merge(db_set1, db_set2)
        result = CppCompilationDatabasesMap.merge(db_set1, db_set_combo)
        self.assertEqual(len(result), 2)
        self.assertCountEqual(result._dbs, {**db_set1._dbs, **db_set2._dbs})


if __name__ == '__main__':
    unittest.main()
