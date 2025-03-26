#!/usr/bin/env python3
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
"""Tests arg parsing and command generation."""

import platform
import unittest
import subprocess
from unittest.mock import patch, Mock
from pathlib import Path


from pw_protobuf_compiler import generate_protos


class TestArgGeneration(unittest.TestCase):
    """Tests the pw_protobuf_compiler.generate_protos method."""

    @staticmethod
    @patch('subprocess.run')
    @patch('pathlib.Path.mkdir')
    @unittest.skipIf(
        platform.system() == "Windows", "Test does not work on windows"
    )
    def test_pwpb(mkdir_mock, subprocess_mock):
        """
        Test for language=pwpb
        """
        mkdir_mock.return_value = None
        subprocess_subprocess_mock_result = Mock()
        subprocess_subprocess_mock_result.returncode = 0
        subprocess_mock.return_value = subprocess_subprocess_mock_result

        args = """
            --out-dir /my/out/dir/for/pwpb
            --compile-dir /path/to/proto
            --language pwpb
            --plugin-path /bin/pw_protobuf_plugin_py
            --proto-path /path/to/other_proto
            --sources /path/to/proto/foo.proto /path/to/other_proto/bar.proto
        """
        generate_protos.main(args.split())
        subprocess_mock.assert_called_once_with(
            (
                Path("protoc"),
                f'-I{Path("/path/to/proto")}',
                f'-I{Path("/path/to/other_proto")}',
                '--experimental_allow_proto3_optional',
                '--experimental_editions',
                '--plugin',
                'protoc-gen-custom=/bin/pw_protobuf_plugin_py',
                f'--custom_opt=-I{Path("/path/to/proto")}',
                f'--custom_opt=-I{Path("/path/to/other_proto")}',
                '--custom_out',
                Path('/my/out/dir/for/pwpb'),
                Path('/path/to/proto/foo.proto'),
                Path('/path/to/other_proto/bar.proto'),
            ),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    @staticmethod
    @patch('subprocess.run')
    @patch('pathlib.Path.mkdir')
    @unittest.skipIf(
        platform.system() == "Windows", "Test does not work on windows"
    )
    def test_pwpb_no_compile_dir(mkdir_mock, subprocess_mock):
        """
        Test for language=pwpb, no use of --compile-dir
        """
        mkdir_mock.return_value = None
        subprocess_subprocess_mock_result = Mock()
        subprocess_subprocess_mock_result.returncode = 0
        subprocess_mock.return_value = subprocess_subprocess_mock_result

        args = """
            --out-dir /my/out/dir/for/pwpb
            --language pwpb
            --plugin-path /bin/pw_protobuf_plugin_py
            --proto-path /path/to/proto
            --proto-path /path/to/other_proto
            --sources /path/to/proto/foo.proto /path/to/other_proto/bar.proto
        """
        generate_protos.main(args.split())
        subprocess_mock.assert_called_once_with(
            (
                Path("protoc"),
                f'-I{Path("/path/to/proto")}',
                f'-I{Path("/path/to/other_proto")}',
                '--experimental_allow_proto3_optional',
                '--experimental_editions',
                '--plugin',
                'protoc-gen-custom=/bin/pw_protobuf_plugin_py',
                f'--custom_opt=-I{Path("/path/to/proto")}',
                f'--custom_opt=-I{Path("/path/to/other_proto")}',
                '--custom_out',
                Path('/my/out/dir/for/pwpb'),
                Path('/path/to/proto/foo.proto'),
                Path('/path/to/other_proto/bar.proto'),
            ),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    @staticmethod
    @patch('subprocess.run')
    @patch('pathlib.Path.mkdir')
    @unittest.skipIf(
        platform.system() == "Windows", "Test does not work on windows"
    )
    def test_nanopb(mkdir_mock, subprocess_mock):
        """
        Test for language=nanopb
        """
        mkdir_mock.return_value = None
        subprocess_mock_result = Mock()
        subprocess_mock_result.returncode = 0
        subprocess_mock.return_value = subprocess_mock_result

        args = """
            --out-dir /my/out/dir/for/nanopb
            --compile-dir /path/to/proto
            --language nanopb
            --plugin-path /bin/pw_protobuf_plugin_py
            --proto-path /path/to/other_proto
            --sources /path/to/proto/foo.proto /path/to/other_proto/bar.proto
        """
        generate_protos.main(args.split())
        subprocess_mock.assert_called_once_with(
            (
                Path("protoc"),
                f'-I{Path("/path/to/proto")}',
                f'-I{Path("/path/to/other_proto")}',
                '--experimental_allow_proto3_optional',
                '--experimental_editions',
                '--plugin',
                'protoc-gen-nanopb=/bin/pw_protobuf_plugin_py',
                '--nanopb_opt=-I/path/to/proto',
                f'--nanopb_out={Path("/my/out/dir/for/nanopb")}',
                Path('/path/to/proto/foo.proto'),
                Path('/path/to/other_proto/bar.proto'),
            ),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    @unittest.skipIf(
        platform.system() == "Windows", "Test does not work on windows"
    )
    def test_nanopb_no_compile_dir(self):
        """
        Test for language=nanopb, no use of --compile-dir
        """
        args = """
            --out-dir /my/out/dir/for/nanopb
            --language nanopb
            --plugin-path /bin/pw_protobuf_plugin_py
            --proto-path /path/to/proto
            --proto-path /path/to/other_proto
            --sources /path/to/proto/foo.proto /path/to/other_proto/bar.proto
        """
        with self.assertRaises(SystemExit):
            generate_protos.main(args.split())


if __name__ == "__main__":
    unittest.main()
