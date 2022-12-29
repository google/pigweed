#!/usr/bin/env python3
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
"""Tests for bazel_parser."""

from pathlib import Path
import tempfile
import unittest

from pw_presubmit import bazel_parser

# This is a real Bazel failure, trimmed slightly.
_REAL_TEST_INPUT = """
Starting local Bazel server and connecting to it...
WARNING: --verbose_explanations has no effect when --explain=<file> is not enabled
Loading:
Loading: 0 packages loaded
Analyzing: 1362 targets (197 packages loaded)
Analyzing: 1362 targets (197 packages loaded, 0 targets configured)
INFO: Analyzed 1362 targets (304 packages loaded, 15546 targets configured).

INFO: Found 1362 targets...
[6 / 124] [Prepa] BazelWorkspaceStatusAction stable-status.txt
[747 / 1,548] Compiling pw_kvs/entry.cc; 0s linux-sandbox ... (3 actions, ...
ERROR: /usr/local/google/home/mohrr/pigweed/pigweed/pw_kvs/BUILD.bazel:25:14: Compiling pw_kvs/entry.cc failed: (Exit 1): gcc failed: error executing command
  (cd /usr/local/google/home/mohrr/.cache/bazel/_bazel_mohrr/7e133e1f95b61... \
  exec env - \
    CPP_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    GCC_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    LD_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    NM_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/... \
    OBJDUMP_TOOL_PATH=external/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_... \
    PATH=/usr/local/google/home/mohrr/pigweed/pigweed/out/host/host_tools:... \
    PWD=/proc/self/cwd \
  external/rules_cc_toolchain/cc_toolchain/wrappers/posix/gcc -MD -MF bazel-out/k8-fastbuild/bin/pw_kvs/_objs/pw_kvs/entry.pic.d '-frandom-seed=bazel-out/k8-fastbuild/bin/pw_kvs/_objs/pw_kvs/entry.pic.o' -fPIC -iquote . -iquote bazel-out/k8-fastbuild/bin -isystem pw_kvs/public -isystem bazel-out/k8-fastbuild/bin/pw_kvs/public -isystem pw_assert/assert_compatibility_public_overrides -isystem bazel-out/k8-fastbuild/bin/pw_assert/assert_compatibility_public_overrides -isystem pw_assert/public -isystem bazel-out/k8-fastbuild/bin/pw_assert/public -isystem pw_preprocessor/public -isystem bazel-out/k8-fastbuild/bin/pw_preprocessor/public -isystem pw_assert_basic/public -isystem bazel-out/k8-fastbuild/bin/pw_assert_basic/public -isystem pw_assert_basic/public_overrides -isystem bazel-out/k8-fastbuild/bin/pw_assert_basic/public_overrides -isystem pw_string/public -isystem bazel-out/k8-fastbuild/bin/pw_string/public -isystem pw_span/public -isystem bazel-out/k8-fastbuild/bin/pw_span/public -isystem pw_polyfill/public -isystem bazel-out/k8-fastbuild/bin/pw_polyfill/public -isystem pw_status/public -isystem bazel-out/k8-fastbuild/bin/pw_status/public -isystem pw_result/public -isystem bazel-out/k8-fastbuild/bin/pw_result/public -isystem pw_sys_io/public -isystem bazel-out/k8-fastbuild/bin/pw_sys_io/public -isystem pw_bytes/public -isystem bazel-out/k8-fastbuild/bin/pw_bytes/public -isystem pw_containers/public -isystem bazel-out/k8-fastbuild/bin/pw_containers/public -isystem pw_checksum/public -isystem bazel-out/k8-fastbuild/bin/pw_checksum/public -isystem pw_compilation_testing/public -isystem bazel-out/k8-fastbuild/bin/pw_compilation_testing/public -isystem pw_log/public -isystem bazel-out/k8-fastbuild/bin/pw_log/public -isystem pw_log_basic/public -isystem bazel-out/k8-fastbuild/bin/pw_log_basic/public -isystem pw_log_basic/public_overrides -isystem bazel-out/k8-fastbuild/bin/pw_log_basic/public_overrides -isystem pw_stream/public -isystem bazel-out/k8-fastbuild/bin/pw_stream/public -nostdinc++ -nostdinc -isystemexternal/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/include/c++/v1 -isystemexternal/debian_stretch_amd64_sysroot/usr/local/include -isystemexternal/debian_stretch_amd64_sysroot/usr/include/x86_64-linux-gnu -isystemexternal/debian_stretch_amd64_sysroot/usr/include -isystemexternal/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/lib/clang/12.0.0 -isystemexternal/clang_llvm_12_00_x86_64_linux_gnu_ubuntu_16_04/lib/clang/12.0.0/include -fdata-sections -ffunction-sections -no-canonical-prefixes -Wno-builtin-macro-redefined '-D__DATE__="redacted"' '-D__TIMESTAMP__="redacted"' '-D__TIME__="redacted"' -xc++ --sysroot external/debian_stretch_amd64_sysroot -O0 -fPIC -g -fno-common -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Werror '-Wno-error=cpp' '-Wno-error=deprecated-declarations' -Wno-private-header '-std=c++17' -fno-rtti -Wnon-virtual-dtor -Wno-register -c pw_kvs/entry.cc -o bazel-out/k8-fastbuild/bin/pw_kvs/_objs/pw_kvs/entry.pic.o)
# Configuration: 752863e407a197a5b9da05cfc572e7013efd6958e856cee61d2fa474ed...
# Execution platform: @local_config_platform//:host

Use --sandbox_debug to see verbose messages from the sandbox and retain the sandbox build root for debugging
pw_kvs/entry.cc:49:20: error: no member named 'Dat' in 'pw::Status'
    return Status::Dat aLoss();
           ~~~~~~~~^
pw_kvs/entry.cc:49:23: error: expected ';' after return statement
    return Status::Dat aLoss();
                      ^
                      ;
2 errors generated.
INFO: Elapsed time: 5.662s, Critical Path: 1.01s
INFO: 12 processes: 12 internal.
FAILED: Build did NOT complete successfully
FAILED: Build did NOT complete successfully
"""

_REAL_TEST_SUMMARY = """
ERROR: /usr/local/google/home/mohrr/pigweed/pigweed/pw_kvs/BUILD.bazel:25:14: Compiling pw_kvs/entry.cc failed: (Exit 1): gcc failed: error executing command
# Execution platform: @local_config_platform//:host

pw_kvs/entry.cc:49:20: error: no member named 'Dat' in 'pw::Status'
    return Status::Dat aLoss();
           ~~~~~~~~^
pw_kvs/entry.cc:49:23: error: expected ';' after return statement
    return Status::Dat aLoss();
                      ^
                      ;
2 errors generated.
"""

_STOP = 'INFO:\n'

# pylint: disable=attribute-defined-outside-init


class TestBazelParser(unittest.TestCase):
    """Test bazel_parser."""

    def _run(self, contents: str) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            path = Path(tempdir) / 'foo'

            with path.open('w') as outs:
                outs.write(contents)

            self.output = bazel_parser.parse_bazel_stdout(path)

    def test_simple(self) -> None:
        error = 'ERROR: abc\nerror 1\nerror2\n'
        self._run('[0/10] foo\n[1/10] bar\n' + error + _STOP)
        self.assertEqual(error.strip(), self.output.strip())

    def test_path(self) -> None:
        error_in = 'ERROR: abc\n PATH=... \\\nerror 1\nerror2\n'
        error_out = 'ERROR: abc\nerror 1\nerror2\n'
        self._run('[0/10] foo\n[1/10] bar\n' + error_in + _STOP)
        self.assertEqual(error_out.strip(), self.output.strip())

    def test_unterminated(self) -> None:
        error = 'ERROR: abc\nerror 1\nerror 2\n'
        self._run('[0/10] foo\n[1/10] bar\n' + error)
        self.assertEqual(error.strip(), self.output.strip())

    def test_failure(self) -> None:
        self._run(_REAL_TEST_INPUT)
        self.assertEqual(_REAL_TEST_SUMMARY.strip(), self.output.strip())


if __name__ == '__main__':
    unittest.main()
