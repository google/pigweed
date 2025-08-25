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
"""Tests for compile_commands_generator."""

import unittest
import os
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

from pw_ide.compile_commands_generator import (
    apply_virtual_include_fix,
    generate_compile_commands,
    generate_compile_commands_from_aquery_cquery,
    infer_platform_of_action,
    parse_bazel_build_command,
    resolve_bazel_out_paths,
    resolve_virtual_includes_to_real_paths,
    CompileCommand,
    LoggerUI,
)


# Test data needs to be adjusted for Python's os.path.sep
def fix_path_separator(p):
    """Fixes path separators for the current OS."""
    return p.replace('/', os.path.sep)


def fix_path_separators(paths):
    """Fixes path separators for a list of paths."""
    return [fix_path_separator(p) for p in paths]


SAMPLE_AQUERY_ACTION = {
    'targetId': 423,
    'targetLabel': fix_path_separator(
        '//pw_containers/intrusive_map:intrusive_map_test'
    ),
    'actionKey': (
        '990ae6a0baba62cba42e7576fa759c8622188f59632495aaefec66cc77bab3d6'
    ),
    'mnemonic': 'CppCompile',
    'configurationId': 2,
    'arguments': fix_path_separators(
        [
            'external/+_repo_rules5+llvm_toolchain_macos/bin/clang++',
            '--sysroot=external/+_repo_rules3+macos_sysroot',
            '-Wthread-safety',
            '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
            '-g',
            '-fno-common',
            '-fno-exceptions',
            '-ffunction-sections',
            '-fdata-sections',
            '-no-canonical-prefixes',
            '-fno-rtti',
            '-Wno-register',
            '-Wnon-virtual-dtor',
            '-Wall',
            '-Wextra',
            '-Wimplicit-fallthrough',
            '-Wcast-qual',
            '-Wpointer-arith',
            '-Wshadow',
            '-Wredundant-decls',
            '-Werror',
            '-Wno-error=cpp',
            '-Wno-error=deprecated-declarations',
            '-fdiagnostics-color',
            '-MD',
            '-MF',
            'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/'
            'intrusive_map_test/intrusive_map_test.pic.d',
            '-frandom-seed=bazel-out/darwin_arm64-fastbuild/bin/'
            'pw_containers/_objs/intrusive_map_test/'
            'intrusive_map_test.pic.o',
            '-fPIC',
            '-DPW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION=1',
            '-DPW_STATUS_CFG_CHECK_IF_USED=1',
            '-iquote',
            '.',
            '-iquote',
            'bazel-out/darwin_arm64-fastbuild/bin',
            '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
            '_virtual_includes/intrusive_map',
            '-c',
            'pw_containers/intrusive_map_test.cc',
            '-o',
            'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/'
            'intrusive_map_test/intrusive_map_test.pic.o',
        ]
    ),
    'environmentVariables': [
        {'key': 'PATH', 'value': '/bin:/usr/bin:/usr/local/bin'},
        {'key': 'PWD', 'value': '/proc/self/cwd'},
    ],
    'discoversInputs': True,
    'executionPlatform': '@@platforms//host:host',
    'platform': 'darwin_arm64-fastbuild',
}

SAMPLE_CQUERY_HEADERS = [
    (
        fix_path_separator('//pw_containers:intrusive_map'),
        fix_path_separators(
            [
                'pw_containers/public/pw_containers/intrusive_map.h',
                'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
                '_virtual_includes/intrusive_map/pw_containers/'
                'intrusive_map.h',
            ]
        ),
    )
]


class MockLoggerUI(LoggerUI):
    """Mock logger for testing."""

    def __init__(self):
        super().__init__()
        self.stdout = ""
        self.stderr = ""

    def add_stdout(self, data):
        self.stdout += data

    def add_stderr(self, data):
        self.stderr += data

    def get_stdout(self):
        return self.stdout

    def get_stderr(self):
        return self.stderr


class CompileCommandsGeneratorTest(unittest.TestCase):
    """Tests for the compile commands generator."""

    def setUp(self):
        self.bazel_path = "bazel"  # Assume bazel is in the PATH
        self.cwd = Path.cwd()

    def test_infer_platform_of_action(self):
        self.assertEqual(
            infer_platform_of_action(SAMPLE_AQUERY_ACTION),
            'darwin_arm64-fastbuild',
        )

    def test_resolve_virtual_includes_to_real_paths(self):
        cquery_header_with_no_virtual_include = [
            (
                '//pw_containers:intrusive_map',
                fix_path_separators(
                    [
                        'pw_containers/public/pw_containers/intrusive_map.h',
                        'pw_containers/public/pw_containers/intrusive_map.h',
                    ]
                ),
            )
        ]
        map_ = resolve_virtual_includes_to_real_paths(SAMPLE_CQUERY_HEADERS)
        map_empty = resolve_virtual_includes_to_real_paths(
            cquery_header_with_no_virtual_include
        )
        self.assertEqual(
            list(map_.values())[0], fix_path_separator('pw_containers/public')
        )
        self.assertEqual(len(map_empty), 0)

    def test_apply_virtual_include_fix(self):
        headers_map = resolve_virtual_includes_to_real_paths(
            SAMPLE_CQUERY_HEADERS
        )
        sample_command = CompileCommand(
            file='test.cc',
            directory='.',
            arguments=[
                '-I'
                + fix_path_separator(
                    'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
                    '_virtual_includes/intrusive_map'
                ),
                'test.cc',
            ],
        )
        with patch('pathlib.Path.exists', return_value=True):
            fixed_command = apply_virtual_include_fix(
                sample_command, headers_map
            )
        self.assertNotIn(
            '-I'
            + fix_path_separator(
                'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
                '_virtual_includes/intrusive_map'
            ),
            fixed_command.arguments,
        )
        self.assertIn(
            '-I' + fix_path_separator('pw_containers/public'),
            fixed_command.arguments,
        )

    def test_apply_virtual_include_fix_with_nonexistent_real_path(self):
        """Test that virtual paths are not replaced if the real path is fake."""
        headers_map = {
            fix_path_separator(
                'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
                '_virtual_includes/intrusive_map'
            ): fix_path_separator('nonexistent/path')
        }
        sample_command = CompileCommand(
            file='test.cc',
            directory='.',
            arguments=[
                '-I'
                + fix_path_separator(
                    'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
                    '_virtual_includes/intrusive_map'
                ),
                'test.cc',
            ],
        )
        # Patch Path.exists to always return False
        with patch('pathlib.Path.exists', return_value=False):
            fixed_command = apply_virtual_include_fix(
                sample_command, headers_map
            )
        # The virtual path should NOT be replaced because the real path is fake
        self.assertIn(
            '-I'
            + fix_path_separator(
                'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/'
                '_virtual_includes/intrusive_map'
            ),
            fixed_command.arguments,
        )
        self.assertNotIn(
            '-I' + fix_path_separator('nonexistent/path'),
            fixed_command.arguments,
        )

    def test_generate_compile_commands_from_aquery_cquery(self):
        with patch('pathlib.Path.exists', return_value=True):
            compile_commands_per_platform = (
                generate_compile_commands_from_aquery_cquery(
                    str(self.cwd),
                    [SAMPLE_AQUERY_ACTION],
                    SAMPLE_CQUERY_HEADERS,
                    Path('/tmp/bazel-out'),
                )
            )
            self.assertIn(
                'darwin_arm64-fastbuild', compile_commands_per_platform.maps
            )
            db = compile_commands_per_platform.get('darwin_arm64-fastbuild')
            self.assertEqual(len(db.db), 1)
            self.assertEqual(
                db.db[0].file,
                fix_path_separator('pw_containers/intrusive_map_test.cc'),
            )

    def test_resolve_bazel_out_paths(self):
        """Test that bazel-out paths are resolved to real paths."""
        # Use a platform-specific absolute path for testing.
        real_bazel_out = Path(os.path.abspath('/real/bazel-out'))
        if sys.platform == 'win32':
            real_bazel_out = Path('C:\\real\\bazel-out')

        sample_command = CompileCommand(
            file='bazel-out/some/file.cc',
            directory='.',
            arguments=[
                'clang++',
                '-Ibazel-out/include/path',
                'bazel-out/some/file.cc',
                '-o',
                'bazel-out/some/file.o',
            ],
        )

        resolved_command = resolve_bazel_out_paths(
            sample_command, real_bazel_out
        )

        expected_file = real_bazel_out.joinpath('some', 'file.cc')
        self.assertEqual(resolved_command.file, str(expected_file))

        expected_args = [
            'clang++',
            '-I' + str(real_bazel_out.joinpath('include', 'path')),
            str(real_bazel_out.joinpath('some', 'file.cc')),
            '-o',
            str(real_bazel_out.joinpath('some', 'file.o')),
        ]
        self.assertEqual(resolved_command.arguments, expected_args)

    @patch('subprocess.run')
    def test_parse_bazel_build_command_single_target_no_args(self, mock_run):
        mock_run.return_value.stdout = ""
        command = "build //pw_status/..."
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/...'])
        self.assertEqual(args, [])

    @patch('subprocess.run')
    def test_parse_bazel_build_command_single_target_with_at_no_args(
        self, mock_run
    ):
        mock_run.return_value.stdout = ""
        command = "build @pigweed//pw_status/..."
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['@pigweed//pw_status/...'])
        self.assertEqual(args, [])

    @patch('subprocess.run')
    def test_parse_bazel_build_command_multiple_targets_no_args(self, mock_run):
        mock_run.return_value.stdout = ""
        command = "build //pw_string:pw_string //pw_status/ts:js_files"
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(
            targets, ['//pw_string:pw_string', '//pw_status/ts:js_files']
        )
        self.assertEqual(args, [])

    @patch('subprocess.run')
    def test_parse_bazel_build_command_one_config_arg_one_target(
        self, mock_run
    ):
        mock_run.return_value.stdout = "--config=rp2040"
        command = "build --config rp2040 //pw_status/..."
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/...'])
        self.assertEqual(args, ['--config=rp2040'])

    @patch('subprocess.run')
    def test_parse_bazel_platform_arg_and_target_with_test(self, mock_run):
        mock_run.return_value.stdout = "--platforms=@pigweed//targets/rp2040"
        command = (
            "test --platforms=@pigweed//targets/rp2040 //pw_string:pw_string"
        )
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_string:pw_string'])
        self.assertEqual(args, ['--platforms=@pigweed//targets/rp2040'])

    @patch('subprocess.run')
    def test_parse_bazel_build_command_both_args_multiple_targets(
        self, mock_run
    ):
        mock_run.return_value.stdout = (
            "--config=rp2040\n--platforms=@pigweed//targets/rp2040"
        )
        command = (
            "build --config rp2040 --platforms=@pigweed//targets/rp2040 "
            "//pw_status/... //pw_string/..."
        )
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/...', '//pw_string/...'])
        self.assertEqual(
            sorted(args),
            sorted(['--config=rp2040', '--platforms=@pigweed//targets/rp2040']),
        )

    @patch('subprocess.run')
    def test_parse_bazel_build_command_target_then_config_arg(self, mock_run):
        mock_run.return_value.stdout = "--config=rp2040"
        command = "build //pw_status/ts:js_files --config rp2040"
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/ts:js_files'])
        self.assertEqual(args, ['--config=rp2040'])

    @patch('subprocess.run')
    def test_parse_bazel_build_command_run_then_arg(self, mock_run):
        mock_run.return_value.stdout = ""
        command = "run //:format -- --check"
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//:format'])
        self.assertEqual(args, [])

    @patch('subprocess.run')
    def test_parse_bazel_build_command_arg_target_arg_target(self, mock_run):
        mock_run.return_value.stdout = (
            "--config=rp2040\n--platforms=@pigweed//targets/rp2040"
        )
        command = (
            "build --config rp2040 //pw_status/... "
            "--platforms=@pigweed//targets/rp2040 //pw_string:pw_string"
        )
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/...', '//pw_string:pw_string'])
        self.assertEqual(
            sorted(args),
            sorted(['--config=rp2040', '--platforms=@pigweed//targets/rp2040']),
        )

    @patch('subprocess.run')
    def test_parse_bazel_build_command_platform_arg_in_quotes(self, mock_run):
        mock_run.return_value.stdout = (
            '--define=VAR="some value with spaces"\n'
            '--platforms=@pigweed//targets/rp2040'
        )
        command = (
            'test --platforms=@pigweed//targets/rp2040 '
            '--define=VAR="some value with spaces" //pw_status/ts:js_files'
        )
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/ts:js_files'])
        self.assertEqual(
            sorted(args),
            sorted(
                [
                    '--define=VAR="some value with spaces"',
                    '--platforms=@pigweed//targets/rp2040',
                ]
            ),
        )

    @patch('subprocess.run')
    def test_parse_bazel_build_command_platform_arg_before_config_arg(
        self, mock_run
    ):
        mock_run.return_value.stdout = (
            "--config=rp2040\n--platforms=@pigweed//targets/rp2040"
        )
        command = (
            "test --platforms=@pigweed//targets/rp2040 --config rp2040 "
            "//pw_status/ts:js_files"
        )
        targets, args = parse_bazel_build_command(
            command, self.bazel_path, str(self.cwd)
        )
        self.assertEqual(targets, ['//pw_status/ts:js_files'])
        self.assertEqual(
            sorted(args),
            sorted(['--config=rp2040', '--platforms=@pigweed//targets/rp2040']),
        )

    def test_parse_bazel_build_command_error_no_targets_with_specified_args(
        self,
    ):
        command = "build --config rp2040 --platforms=@pigweed//targets/rp2040"
        with self.assertRaisesRegex(
            ValueError, "Could not find any bazel targets"
        ):
            parse_bazel_build_command(command, self.bazel_path, str(self.cwd))

    def test_parse_bazel_build_command_only_subcommand_build(self):
        with self.assertRaisesRegex(
            ValueError, "Could not find any bazel targets"
        ):
            parse_bazel_build_command("build", self.bazel_path, str(self.cwd))

    def test_parse_bazel_build_command_error_empty_command_string(self):
        with self.assertRaisesRegex(ValueError, "Invalid bazel command"):
            parse_bazel_build_command("", self.bazel_path, str(self.cwd))

    @patch('subprocess.check_output')
    @patch(
        'pw_ide.compile_commands_generator.'
        'ensure_external_workspaces_link_exists'
    )
    def test_generate_empty_commands_does_not_delete(
        self, _mock_link_exists, mock_check_output
    ):
        """Test that existing compile_commands are not deleted."""
        mock_check_output.return_value = '/tmp/execroot'
        mock_logger = MockLoggerUI()

        def fake_aquery_runner(*_args, **_kwargs):
            return '', 0

        def fake_cquery_runner(*_args, **_kwargs):
            return '', 0

        # Setup existing compile_commands.json
        cdb_file_dir_name = '.compile_commands_test_dir'
        platform_name = 'test_platform'
        cdb_filename = 'compile_commands.json'
        full_cdb_dir_path = self.cwd / cdb_file_dir_name
        platform_dir_path = full_cdb_dir_path / platform_name
        platform_dir_path.mkdir(parents=True, exist_ok=True)
        existing_cdb_file_path = platform_dir_path / cdb_filename
        existing_content = '[{"file": "old.c", "command": "gcc old.c"}]'
        existing_cdb_file_path.write_text(existing_content)

        generate_compile_commands(
            self.bazel_path,
            str(self.cwd),
            cdb_file_dir_name,
            cdb_filename,
            [],  # Empty bazelTargets
            [],
            fake_aquery_runner,
            fake_cquery_runner,
            mock_logger,
        )

        self.assertTrue(existing_cdb_file_path.exists())
        self.assertEqual(existing_cdb_file_path.read_text(), existing_content)
        self.assertIn(
            'No compile commands generated.', mock_logger.get_stdout()
        )

    @patch('subprocess.check_output')
    @patch(
        'pw_ide.compile_commands_generator.'
        'ensure_external_workspaces_link_exists'
    )
    def test_generate_compile_commands_aquery_fails_stops_execution(
        self, _mock_link_exists, mock_check_output
    ):
        """Test that aquery failures stop execution."""
        mock_check_output.return_value = '/tmp/execroot'
        mock_logger = MockLoggerUI()

        def fake_aquery_runner_fails(*_args, **_kwargs):
            # Simulate a failure in aquery
            return 'aquery error', 1

        mock_cquery_runner = MagicMock(return_value=('', 0))

        cdb_file_dir_name = '.compile_commands_test_dir'
        cdb_filename = 'compile_commands.json'

        with self.assertRaises(RuntimeError):
            generate_compile_commands(
                self.bazel_path,
                str(self.cwd),
                cdb_file_dir_name,
                cdb_filename,
                ['//...'],
                [],
                fake_aquery_runner_fails,
                mock_cquery_runner,
                mock_logger,
            )

        mock_cquery_runner.assert_not_called()
        self.assertIn(
            'aquery failed for //... with exit code 1',
            mock_logger.get_stderr(),
        )

    def test_generate_compile_commands_from_aquery_cquery_with_nonexistent_file(
        self,
    ):
        """Test that nonexistent files are handled gracefully."""
        mock_logger = MockLoggerUI()
        with patch('pathlib.Path.exists', return_value=False):
            compile_commands_per_platform = (
                generate_compile_commands_from_aquery_cquery(
                    str(self.cwd),
                    [SAMPLE_AQUERY_ACTION],
                    SAMPLE_CQUERY_HEADERS,
                    Path('/tmp/bazel-out'),
                    mock_logger,
                )
            )
            self.assertIn(
                'darwin_arm64-fastbuild', compile_commands_per_platform.maps
            )
            warning_msg = (
                'Warning: Source file '
                f'{fix_path_separator("pw_containers/intrusive_map_test.cc")} '
                'does not exist.'
            )
            self.assertIn(
                warning_msg,
                mock_logger.get_stderr(),
            )


if __name__ == '__main__':
    unittest.main()
