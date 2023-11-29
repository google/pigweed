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
"""Tests for ninja_parser."""

import difflib
from pathlib import Path
import unittest
from unittest.mock import MagicMock, mock_open, patch

from pw_presubmit import ninja_parser

_STOP = 'ninja: build stopped:\n'

_REAL_BUILD_INPUT = """
[1168/1797] cp ../../pw_software_update/py/dev_sign_test.py python/gen/pw_software_update/py/py.generated_python_package/dev_sign_test.py
[1169/1797] ACTION //pw_presubmit/py:py.lint.mypy(//pw_build/python_toolchain:python)
\x1b[31mFAILED: python/gen/pw_presubmit/py/py.lint.mypy.pw_pystamp
python ../../pw_build/py/pw_build/python_runner.py --gn-root ../../ --current-path ../../pw_presubmit/py --default-toolchain=//pw_toolchain/default:default --current-toolchain=//pw_build/python_toolchain:python --env=MYPY_FORCE_COLOR=1 --touch python/gen/pw_presubmit/py/py.lint.mypy.pw_pystamp --capture-output --module mypy --python-virtualenv-config python/gen/pw_env_setup/pigweed_build_venv/venv_metadata.json --python-dep-list-files python/gen/pw_presubmit/py/py.lint.mypy_metadata_path_list.txt -- --pretty --show-error-codes ../../pw_presubmit/py/pw_presubmit/__init__.py ../../pw_presubmit/py/pw_presubmit/build.py ../../pw_presubmit/py/pw_presubmit/cli.py ../../pw_presubmit/py/pw_presubmit/cpp_checks.py ../../pw_presubmit/py/pw_presubmit/format_code.py ../../pw_presubmit/py/pw_presubmit/git_repo.py ../../pw_presubmit/py/pw_presubmit/inclusive_language.py ../../pw_presubmit/py/pw_presubmit/install_hook.py ../../pw_presubmit/py/pw_presubmit/keep_sorted.py ../../pw_presubmit/py/pw_presubmit/ninja_parser.py ../../pw_presubmit/py/pw_presubmit/npm_presubmit.py ../../pw_presubmit/py/pw_presubmit/pigweed_presubmit.py ../../pw_presubmit/py/pw_presubmit/presubmit.py ../../pw_presubmit/py/pw_presubmit/python_checks.py ../../pw_presubmit/py/pw_presubmit/shell_checks.py ../../pw_presubmit/py/pw_presubmit/todo_check.py ../../pw_presubmit/py/pw_presubmit/tools.py ../../pw_presubmit/py/git_repo_test.py ../../pw_presubmit/py/keep_sorted_test.py ../../pw_presubmit/py/ninja_parser_test.py ../../pw_presubmit/py/presubmit_test.py ../../pw_presubmit/py/tools_test.py ../../pw_presubmit/py/setup.py
  Requirement already satisfied: pyserial in c:\\b\\s\\w\\ir\\x\\w\\co\\environment\\pigweed-venv\\lib\\site-packages (from pigweed==0.0.13+20230126212203) (3.5)
../../pw_presubmit/py/presubmit_test.py:63: error: Module has no attribute
"Filter"  [attr-defined]
            TestData(presubmit.Filter(suffix=('.a', '.b')), 'foo.c', False...
                     ^
Found 1 error in 1 file (checked 23 source files)
[1170/1797] stamp python/obj/pw_snapshot/metadata_proto.python._mirror_sources_to_out_dir.stamp
[1171/1797] stamp python/obj/pw_software_update/py/py._mirror_sources_to_out_dir_dev_sign_test.py.stamp
[1172/1797] ACTION //pw_log:protos.python(//pw_build/python_toolchain:python)
[1173/1797] ACTION //pw_thread_freertos/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1174/1797] ACTION //pw_symbolizer/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1175/1797] ACTION //pw_symbolizer/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1176/1797] ACTION //pw_thread_freertos/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1177/1797] ACTION //pw_tls_client/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1178/1797] ACTION //pw_symbolizer/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1179/1797] ACTION //pw_console/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1180/1797] ACTION //pw_tls_client/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1181/1797] ACTION //pw_console/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1182/1797] ACTION //pw_console/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1183/1797] ACTION //pw_tls_client/py:py.lint.mypy(//pw_build/python_toolchain:python)
[1184/1797] ACTION //pw_symbolizer/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1185/1797] ACTION //pw_thread_freertos/py:py.lint.pylint(//pw_build/python_toolchain:python)
[1186/1797] ACTION //pw_tls_client/py:py.lint.pylint(//pw_build/python_toolchain:python)
ninja: build stopped: subcommand failed.
[FINISHED]
"""

_REAL_BUILD_SUMMARY = """
[1169/1797] ACTION //pw_presubmit/py:py.lint.mypy(//pw_build/python_toolchain:python)
\x1b[31mFAILED: python/gen/pw_presubmit/py/py.lint.mypy.pw_pystamp
python ../../pw_build/py/pw_build/python_runner.py --gn-root ../../ --current-path ../../pw_presubmit/py --default-toolchain=//pw_toolchain/default:default --current-toolchain=//pw_build/python_toolchain:python --env=MYPY_FORCE_COLOR=1 --touch python/gen/pw_presubmit/py/py.lint.mypy.pw_pystamp --capture-output --module mypy --python-virtualenv-config python/gen/pw_env_setup/pigweed_build_venv/venv_metadata.json --python-dep-list-files python/gen/pw_presubmit/py/py.lint.mypy_metadata_path_list.txt -- --pretty --show-error-codes ../../pw_presubmit/py/pw_presubmit/__init__.py ../../pw_presubmit/py/pw_presubmit/build.py ../../pw_presubmit/py/pw_presubmit/cli.py ../../pw_presubmit/py/pw_presubmit/cpp_checks.py ../../pw_presubmit/py/pw_presubmit/format_code.py ../../pw_presubmit/py/pw_presubmit/git_repo.py ../../pw_presubmit/py/pw_presubmit/inclusive_language.py ../../pw_presubmit/py/pw_presubmit/install_hook.py ../../pw_presubmit/py/pw_presubmit/keep_sorted.py ../../pw_presubmit/py/pw_presubmit/ninja_parser.py ../../pw_presubmit/py/pw_presubmit/npm_presubmit.py ../../pw_presubmit/py/pw_presubmit/pigweed_presubmit.py ../../pw_presubmit/py/pw_presubmit/presubmit.py ../../pw_presubmit/py/pw_presubmit/python_checks.py ../../pw_presubmit/py/pw_presubmit/shell_checks.py ../../pw_presubmit/py/pw_presubmit/todo_check.py ../../pw_presubmit/py/pw_presubmit/tools.py ../../pw_presubmit/py/git_repo_test.py ../../pw_presubmit/py/keep_sorted_test.py ../../pw_presubmit/py/ninja_parser_test.py ../../pw_presubmit/py/presubmit_test.py ../../pw_presubmit/py/tools_test.py ../../pw_presubmit/py/setup.py
../../pw_presubmit/py/presubmit_test.py:63: error: Module has no attribute
"Filter"  [attr-defined]
            TestData(presubmit.Filter(suffix=('.a', '.b')), 'foo.c', False...
                     ^
Found 1 error in 1 file (checked 23 source files)
"""

_REAL_TEST_INPUT = r"""
[20278/31188] Finished [ld pw_strict_host_clang_size_optimized/obj/pw_rpc/bin/test_rpc_server] (0.0s)
[20278/31188] Started  [c++ pw_strict_host_clang_size_optimized/obj/pw_rpc/pwpb/fake_channel_output_test.lib.fake_channel_output_test.cc.o]
[20279/31188] Finished [c++ pw_strict_host_clang_size_optimized/obj/pw_log_rpc/log_service.log_service.cc.o] (4.9s)
[20279/31188] Started  [c++ pw_strict_host_clang_size_optimized/obj/pw_rpc/pwpb/method_info_test.lib.method_info_test.cc.o]
[20280/31188] Finished [ACTION //pw_rpc:cpp_client_server_integration_test(//targets/host/pigweed_internal:pw_strict_host_clang_debug)] (10.2s)
[ACTION //pw_rpc:cpp_client_server_integration_test(//targets/host/pigweed_internal:pw_strict_host_clang_debug)]
[31mFAILED: [0mpw_strict_host_clang_debug/gen/pw_rpc/cpp_client_server_integration_test.pw_pystamp
python ../../pw_build/py/pw_build/python_runner.py --gn-root ../../ --current-path ../../pw_rpc --default-toolchain=//pw_toolchain/default:default --current-toolchain=//targets/host/pigweed_internal:pw_strict_host_clang_debug --touch pw_strict_host_clang_debug/gen/pw_rpc/cpp_client_server_integration_test.pw_pystamp --capture-output --python-virtualenv-config python/gen/pw_env_setup/pigweed_build_venv/venv_metadata.json --python-dep-list-files pw_strict_host_clang_debug/gen/pw_rpc/cpp_client_server_integration_test_metadata_path_list.txt -- ../../pw_rpc/py/pw_rpc/testing.py --server \<TARGET_FILE\(:test_rpc_server\)\> --client \<TARGET_FILE\(:client_integration_test\)\> -- 30577
[35m[1mINF[0m  Starting pw_rpc server on port 30577
[35m[1mINF[0m  Connecting to pw_rpc client at localhost:30577
[35m[1mINF[0m  [==========] Running all tests.
[35m[1mINF[0m  [ RUN      ] RawRpcIntegrationTest.Unary
[35m[1mINF[0m  Test output line
[35m[1mINF[0m  [       OK ] RawRpcIntegrationTest.Unary
[35m[1mINF[0m  [ RUN      ] RawRpcIntegrationTest.BidirectionalStreaming
[35m[1mINF[0m  [       OK ] RawRpcIntegrationTest.BidirectionalStreaming
[35m[1mINF[0m  [ RUN      ] RawRpcIntegrationTest.OnNextOverwritesItsOwnCall
[33m[1mWRN[0m  RPC client received stream message for an unknown call
[31m

   ▄████▄      ██▀███      ▄▄▄           ██████     ██░ ██
  ▒██▀ ▀█     ▓██ ▒ ██▒   ▒████▄       ▒██    ▒    ▓██░ ██▒
  ▒▓█ 💥 ▄    ▓██ ░▄█ ▒   ▒██  ▀█▄     ░ ▓██▄      ▒██▀▀██░
  ▒▓▓▄ ▄██▒   ▒██▀▀█▄     ░██▄▄▄▄██      ▒   ██▒   ░▓█ ░██
  ▒ ▓███▀ ░   ░██▓ ▒██▒    ▓█   ▓██▒   ▒██████▒▒   ░▓█▒░██▓
  ░ ░▒ ▒  ░   ░ ▒▓ ░▒▓░    ▒▒   ▓▒█░   ▒ ▒▓▒ ▒ ░    ▒ ░░▒░▒
    ░  ▒        ░▒ ░ ▒░     ▒   ▒▒ ░   ░ ░▒  ░ ░    ▒ ░▒░ ░
  ░             ░░   ░      ░   ▒      ░  ░  ░      ░  ░░ ░
  ░ ░            ░              ░  ░         ░      ░  ░  ░
  ░

[0m
  Welp, that didn't go as planned. It seems we crashed. Terribly sorry!

[33m  CRASH MESSAGE[0m

     Check failed: sem_.try_acquire_for(10s).

[33m  CRASH FILE & LINE[0m

     pw_rpc/client_integration_test.cc:52

[33m  CRASH FUNCTION[0m

     const char *rpc_test::(anonymous namespace)::StringReceiver::Wait()

[20281/31188] Finished [c++ pw_strict_host_clang_size_optimized/obj/pw_log_rpc/rpc_log_drain.rpc_log_drain.cc.o] (5.1s)
[20282/31188] Finished [c++ pw_strict_host_clang_size_optimized/obj/pw_log_rpc/log_filter_service_test.lib.log_filter_service_test.cc.o] (5.9s)
[20283/31188] Finished [c++ pw_strict_host_clang_size_optimized/obj/pw_rpc/fuzz/engine_test.lib.engine_test.cc.o] (2.4s)
"""

_REAL_TEST_SUMMARY = r"""
[ACTION //pw_rpc:cpp_client_server_integration_test(//targets/host/pigweed_internal:pw_strict_host_clang_debug)]
[31mFAILED: [0mpw_strict_host_clang_debug/gen/pw_rpc/cpp_client_server_integration_test.pw_pystamp
python ../../pw_build/py/pw_build/python_runner.py --gn-root ../../ --current-path ../../pw_rpc --default-toolchain=//pw_toolchain/default:default --current-toolchain=//targets/host/pigweed_internal:pw_strict_host_clang_debug --touch pw_strict_host_clang_debug/gen/pw_rpc/cpp_client_server_integration_test.pw_pystamp --capture-output --python-virtualenv-config python/gen/pw_env_setup/pigweed_build_venv/venv_metadata.json --python-dep-list-files pw_strict_host_clang_debug/gen/pw_rpc/cpp_client_server_integration_test_metadata_path_list.txt -- ../../pw_rpc/py/pw_rpc/testing.py --server \<TARGET_FILE\(:test_rpc_server\)\> --client \<TARGET_FILE\(:client_integration_test\)\> -- 30577
[35m[1mINF[0m  Starting pw_rpc server on port 30577
[35m[1mINF[0m  Connecting to pw_rpc client at localhost:30577
[35m[1mINF[0m  [==========] Running all tests.
[35m[1mINF[0m  [ RUN      ] RawRpcIntegrationTest.OnNextOverwritesItsOwnCall
[33m[1mWRN[0m  RPC client received stream message for an unknown call
[31m
   ▄████▄      ██▀███      ▄▄▄           ██████     ██░ ██
  ▒██▀ ▀█     ▓██ ▒ ██▒   ▒████▄       ▒██    ▒    ▓██░ ██▒
  ▒▓█ 💥 ▄    ▓██ ░▄█ ▒   ▒██  ▀█▄     ░ ▓██▄      ▒██▀▀██░
  ▒▓▓▄ ▄██▒   ▒██▀▀█▄     ░██▄▄▄▄██      ▒   ██▒   ░▓█ ░██
  ▒ ▓███▀ ░   ░██▓ ▒██▒    ▓█   ▓██▒   ▒██████▒▒   ░▓█▒░██▓
  ░ ░▒ ▒  ░   ░ ▒▓ ░▒▓░    ▒▒   ▓▒█░   ▒ ▒▓▒ ▒ ░    ▒ ░░▒░▒
    ░  ▒        ░▒ ░ ▒░     ▒   ▒▒ ░   ░ ░▒  ░ ░    ▒ ░▒░ ░
  ░             ░░   ░      ░   ▒      ░  ░  ░      ░  ░░ ░
  ░ ░            ░              ░  ░         ░      ░  ░  ░
  ░
[0m
  Welp, that didn't go as planned. It seems we crashed. Terribly sorry!
[33m  CRASH MESSAGE[0m
     Check failed: sem_.try_acquire_for(10s).
[33m  CRASH FILE & LINE[0m
     pw_rpc/client_integration_test.cc:52
[33m  CRASH FUNCTION[0m
     const char *rpc_test::(anonymous namespace)::StringReceiver::Wait()
"""


class TestNinjaParser(unittest.TestCase):
    """Test ninja_parser."""

    def _run(self, contents: str) -> str:  # pylint: disable=no-self-use
        path = MagicMock(spec=Path('foo/bar'))

        def mocked_open_read(*args, **kwargs):
            return mock_open(read_data=contents)(*args, **kwargs)

        with patch.object(path, 'open', mocked_open_read):
            return ninja_parser.parse_ninja_stdout(path)

    def _assert_equal(self, left, right):
        if (
            not isinstance(left, str)
            or not isinstance(right, str)
            or '\n' not in left
            or '\n' not in right
        ):
            return self.assertEqual(left, right)

        diff = ''.join(
            difflib.unified_diff(left.splitlines(True), right.splitlines(True))
        )
        return self.assertSequenceEqual(left, right, f'\n{diff}\n')

    def test_simple(self) -> None:
        error = '[2/10] baz\nFAILED: something\nerror 1\nerror 2\n'
        result = self._run('[0/10] foo\n[1/10] bar\n' + error + _STOP)
        self._assert_equal(error.strip(), result.strip())

    def test_short(self) -> None:
        error = '[2/10] baz\nFAILED: something\n'
        result = self._run('[0/10] foo\n[1/10] bar\n' + error + _STOP)
        self._assert_equal(error.strip(), result.strip())

    def test_unexpected(self) -> None:
        error = '[2/10] baz\nERROR: something\nerror 1\n'
        result = self._run('[0/10] foo\n[1/10] bar\n' + error)
        self._assert_equal('', result.strip())

    def test_real_build(self) -> None:
        result = self._run(_REAL_BUILD_INPUT)
        self._assert_equal(_REAL_BUILD_SUMMARY.strip(), result.strip())

    def test_real_test(self) -> None:
        result = self._run(_REAL_TEST_INPUT)
        self._assert_equal(_REAL_TEST_SUMMARY.strip(), result.strip())


if __name__ == '__main__':
    unittest.main()
