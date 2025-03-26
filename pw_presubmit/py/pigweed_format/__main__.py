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
"""A CLI utility that checks and fixes formatting for source files."""

import sys

from pw_build.runfiles_manager import RunfilesManager
from pw_presubmit.format.bazel import BuildifierFormatter
from pw_presubmit.format.cpp import ClangFormatFormatter
from pw_presubmit.format.private.cli import FormattingSuite
from pw_presubmit.format.owners import OwnersFormatter
from pw_presubmit.format.python import BlackFormatter


try:
    # pylint: disable=unused-import
    import python.runfiles  # type: ignore

    # pylint: enable=unused-import

    _FORMAT_FIX_COMMAND = 'bazel run @pigweed//pw_presubmit/py:format --'
except ImportError:
    _FORMAT_FIX_COMMAND = 'python -m pigweed_format'


def _pigweed_formatting_suite() -> FormattingSuite:
    runfiles = RunfilesManager()
    # GN
    runfiles.add_bootstrapped_tool(
        'clang-format', 'clang-format', from_shell_path=True
    )
    runfiles.add_bootstrapped_tool('black', 'black', from_shell_path=True)
    runfiles.add_bootstrapped_tool(
        'buildifier', 'buildifier', from_shell_path=True
    )

    # Bazel
    runfiles.add_bazel_tool('clang-format', 'llvm_toolchain.clang_format')
    runfiles.add_bazel_tool('black', 'pw_presubmit.py.black_runfiles')
    runfiles.add_bazel_tool('buildifier', 'pw_presubmit.py.buildifier_runfiles')

    # This list can be broken out and library-ified as the default set of
    # formatters once config file loading is smarter (i.e. loads from the
    # path of the file that is being formatted rather than as a runfile
    # dependency).
    pigweed_formatters = [
        BlackFormatter(
            tool_runner=runfiles,
        ),
        BuildifierFormatter(
            tool_runner=runfiles,
        ),
        ClangFormatFormatter(
            tool_runner=runfiles,
        ),
        OwnersFormatter(
            tool_runner=runfiles,
        ),
    ]
    return FormattingSuite(
        pigweed_formatters,
        formatter_fix_command=_FORMAT_FIX_COMMAND,
    )


if __name__ == '__main__':
    sys.exit(_pigweed_formatting_suite().main())
