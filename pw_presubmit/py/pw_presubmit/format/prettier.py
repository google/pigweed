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
"""Code formatter plugin for web frontend files using Prettier."""

from pathlib import Path
from collections.abc import Iterable, Iterator

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)

# From https://prettier.io/docs/
# This list can be expanded.
DEFAULT_PRETTIER_FILE_PATTERNS = FileFilter(
    endswith=(
        '.js',
        '.mjs',
        '.cjs',
        '.ts',
        '.mts',
        '.cts',
        '.css',
        '.less',
        '.scss',
        '.html',
        '.json',
        '.md',
        '.yaml',
        '.yml',
    )
)


class PrettierFormatter(FileFormatter):
    """A formatter that runs `prettier` on files."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'Prettier')
        kwargs.setdefault('file_patterns', DEFAULT_PRETTIER_FILE_PATTERNS)
        super().__init__(**kwargs)

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Uses ``prettier`` to check the formatting of the requested file."""
        # Prettier determines parser from file extension.
        proc = self.run_tool(
            'prettier',
            ['--stdin-filepath', str(file_path)],
            input=file_contents,
        )
        ok = proc.returncode == 0
        return FormattedFileContents(
            ok=ok,
            formatted_file_contents=proc.stdout,
            error_message=proc.stderr.decode() if not ok else None,
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place using ``prettier``."""
        proc = self.run_tool(
            'prettier',
            [str(file_path), '--write'],
        )
        ok = proc.returncode == 0
        return FormatFixStatus(
            ok=ok,
            error_message=proc.stderr.decode() if not ok else None,
        )

    def format_files(
        self, paths: Iterable[Path], keep_warnings: bool = True
    ) -> Iterator[tuple[Path, FormatFixStatus]]:
        """Uses ``prettier`` to format the specified files in-place."""
        path_list = list(paths)
        if not path_list:
            return
        str_paths = [str(p) for p in path_list]
        proc = self.run_tool(
            'prettier',
            [*str_paths, '--write'],
        )

        if proc.returncode != 0:
            # Fallback to individual files to find the culprit
            yield from super().format_files(path_list, keep_warnings)
