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

from dataclasses import dataclass
import importlib.resources
import sys

from pw_build.runfiles_manager import RunfilesManager
from pw_presubmit.format.core import FileFormatter
from pw_presubmit.format.bazel import BuildifierFormatter
from pw_presubmit.format.cpp import ClangFormatFormatter
from pw_presubmit.format.gn import GnFormatter
from pw_presubmit.format.go import GofmtFormatter
from pw_presubmit.format.java import JavaFormatter
from pw_presubmit.format.json import JsonFormatter
from pw_presubmit.format.private.cli import FormattingSuite
from pw_presubmit.format.owners import OwnersFormatter
from pw_presubmit.format.protobuf import ProtobufFormatter
from pw_presubmit.format.javascript import JavaScriptFormatter
from pw_presubmit.format.python import BlackFormatter
from pw_presubmit.format.rst import RstFormatter
from pw_presubmit.format.rust import RustfmtFormatter
from pw_presubmit.format.starlark import StarlarkFormatter
from pw_presubmit.format.typescript import TypeScriptFormatter
from pw_presubmit.format.cmake import CmakeFormatter
from pw_presubmit.format.css import CssFormatter
from pw_presubmit.format.markdown import MarkdownFormatter


try:
    # pylint: disable=unused-import
    import python.runfiles  # type: ignore

    # pylint: enable=unused-import

    _FORMAT_FIX_COMMAND = 'bazel run @pigweed//pw_presubmit/py:format --'
except ImportError:
    _FORMAT_FIX_COMMAND = 'python -m pigweed_format'


_PACKAGE_DATA_DIR = importlib.resources.files('pigweed_format')
_DISABLED_FORMATTERS = _PACKAGE_DATA_DIR / 'disabled_formatters.txt'


@dataclass
class FormatterSetup:
    formatter: FileFormatter
    binary: None | str
    bazel_import_path: None | str


def _pigweed_formatting_suite() -> FormattingSuite:
    if _DISABLED_FORMATTERS.is_file():
        disabled_formatters = set(_DISABLED_FORMATTERS.read_text().splitlines())
    else:
        disabled_formatters = set()

    runfiles = RunfilesManager()

    all_formatters = [
        FormatterSetup(
            formatter=BlackFormatter(
                tool_runner=runfiles,
            ),
            binary='black',
            bazel_import_path='pw_presubmit.py.black_runfiles',
        ),
        FormatterSetup(
            formatter=BuildifierFormatter(
                tool_runner=runfiles,
            ),
            binary='buildifier',
            bazel_import_path='pw_presubmit.py.buildifier_runfiles',
        ),
        FormatterSetup(
            formatter=ClangFormatFormatter(
                tool_runner=runfiles,
            ),
            binary='clang-format',
            bazel_import_path='llvm_toolchain.clang_format',
        ),
        FormatterSetup(
            formatter=CmakeFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        FormatterSetup(
            formatter=CssFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        FormatterSetup(
            formatter=GnFormatter(
                tool_runner=runfiles,
            ),
            binary='gn',
            bazel_import_path='pw_presubmit.py.gn_runfiles',
        ),
        FormatterSetup(
            formatter=GofmtFormatter(
                tool_runner=runfiles,
            ),
            binary='gofmt',
            bazel_import_path='pw_presubmit.py.gofmt_runfiles',
        ),
        FormatterSetup(
            formatter=JavaScriptFormatter(
                tool_runner=runfiles,
            ),
            binary='prettier',
            bazel_import_path='pw_presubmit.py.prettier_runfiles',
        ),
        FormatterSetup(
            formatter=JavaFormatter(
                tool_runner=runfiles,
            ),
            binary='clang-format',
            bazel_import_path='llvm_toolchain.clang_format',
        ),
        FormatterSetup(
            formatter=JsonFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        FormatterSetup(
            formatter=MarkdownFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        FormatterSetup(
            formatter=OwnersFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        FormatterSetup(
            formatter=ProtobufFormatter(
                tool_runner=runfiles,
            ),
            binary='clang-format',
            bazel_import_path='llvm_toolchain.clang_format',
        ),
        FormatterSetup(
            formatter=RstFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        FormatterSetup(
            formatter=RustfmtFormatter(
                tool_runner=runfiles,
            ),
            binary='rustfmt',
            bazel_import_path='pw_presubmit.py.rustfmt_runfiles',
        ),
        FormatterSetup(
            formatter=StarlarkFormatter(
                tool_runner=runfiles,
            ),
            binary='buildifier',
            bazel_import_path='pw_presubmit.py.buildifier_runfiles',
        ),
        FormatterSetup(
            formatter=TypeScriptFormatter(
                tool_runner=runfiles,
            ),
            binary='prettier',
            bazel_import_path='pw_presubmit.py.prettier_runfiles',
        ),
    ]
    enabled_formatters = []
    for fmt in all_formatters:
        if fmt.formatter.mnemonic not in disabled_formatters:
            enabled_formatters.append(fmt)
        else:
            disabled_formatters.remove(fmt.formatter.mnemonic)

    assert (
        not disabled_formatters
    ), f'Attempted to disable unknown formatters: {disabled_formatters}'

    # Setup runfiles.
    for formatter in enabled_formatters:
        if formatter.binary is not None:
            if formatter.binary in runfiles:
                continue
            runfiles.add_bootstrapped_tool(
                formatter.binary, formatter.binary, from_shell_path=True
            )
            if formatter.bazel_import_path is not None:
                runfiles.add_bazel_tool(
                    formatter.binary, formatter.bazel_import_path
                )

    return FormattingSuite(
        [fmt.formatter for fmt in enabled_formatters],
        formatter_fix_command=_FORMAT_FIX_COMMAND,
    )


if __name__ == '__main__':
    sys.exit(_pigweed_formatting_suite().main())
