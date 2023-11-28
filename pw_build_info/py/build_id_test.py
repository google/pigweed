# Copyright 2021 The Pigweed Authors
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
"""Tests for pw_build_info's GNU build ID support."""

import os
import subprocess
from pathlib import Path
import tempfile
import unittest

from pw_build_info import build_id

# Since build_id.cc depends on pw_preprocessor, we have to use the in-tree path
# to reference the dependent source files.
_MODULE_DIR = Path(__file__).resolve().parent.parent
_MODULE_PY_DIR = Path(__file__).resolve().parent

_SHA1_BUILD_ID_LENGTH = 20


class TestGnuBuildId(unittest.TestCase):
    """Unit tests for GNU build ID parsing."""

    def test_build_id_correctness(self):
        """Tests to ensure GNU build IDs are read/written correctly."""
        self.assertTrue('PW_PIGWEED_CIPD_INSTALL_DIR' in os.environ)
        sysroot = Path(os.environ['PW_PIGWEED_CIPD_INSTALL_DIR']).joinpath(
            "clang_sysroot"
        )
        with tempfile.TemporaryDirectory() as exe_dir:
            exe_file = Path(exe_dir) / 'print_build_id.elf'

            # Compiles a binary that prints the embedded GNU build id.
            cmd = [
                'clang++',
                _MODULE_DIR / 'build_id.cc',
                _MODULE_PY_DIR / 'print_build_id.cc',
                f'-I{_MODULE_DIR}/public',
                f'-I{_MODULE_DIR}/../pw_polyfill/public',
                f'-I{_MODULE_DIR}/../pw_preprocessor/public',
                f'-I{_MODULE_DIR}/../pw_span/public',
                '--sysroot=%s' % sysroot,
                '-std=c++17',
                '-fuse-ld=lld',
                f'-Wl,-T{_MODULE_DIR}/add_build_id_to_default_linker_script.ld',
                f'-Wl,-L{_MODULE_DIR}',
                '-Wl,--build-id=sha1',
                '-o',
                exe_file,
            ]

            process = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            self.assertEqual(
                process.returncode, 0, process.stdout.decode(errors='replace')
            )

            # Run the compiled binary so the printed build ID can be read.
            process = subprocess.run(
                [exe_file],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            self.assertEqual(process.returncode, 0)

            with open(exe_file, 'rb') as elf:
                expected = build_id.read_build_id_from_section(elf)
                self.assertEqual(len(expected), _SHA1_BUILD_ID_LENGTH)
                self.assertEqual(
                    process.stdout.decode().rstrip(), expected.hex()
                )

                # Test method that parses using symbol information.
                expected = build_id.read_build_id_from_symbol(elf)
                self.assertEqual(len(expected), _SHA1_BUILD_ID_LENGTH)
                self.assertEqual(
                    process.stdout.decode().rstrip(), expected.hex()
                )

                # Test the user-facing method.
                expected = build_id.read_build_id(elf)
                self.assertEqual(len(expected), _SHA1_BUILD_ID_LENGTH)
                self.assertEqual(
                    process.stdout.decode().rstrip(), expected.hex()
                )


if __name__ == '__main__':
    unittest.main()
