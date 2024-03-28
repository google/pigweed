# Copyright 2024 The Pigweed Authors
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
"""Code formatter plugin for C/C++."""

from pathlib import Path
from typing import Final, Iterable, Iterator, Sequence

from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)


class ClangFormatFormatter(FileFormatter):
    """A formatter that runs `clang-format` on files."""

    DEFAULT_FLAGS: Final[Sequence[str]] = ('--style=file',)

    def __init__(self, tool_flags: Sequence[str] = DEFAULT_FLAGS, **kwargs):
        super().__init__(**kwargs)
        self.clang_format_flags = list(tool_flags)

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Uses ``clang-format`` to check the formatting of the requested file.

        The file at ``file_path`` is NOT modified by this check.

        Returns:
            A populated
            :py:class:`pw_presubmit.format.core.FormattedFileContents` that
            contains either the result of formatting the file, or an error
            message.
        """
        proc = self.run_tool(
            'clang-format',
            self.clang_format_flags + [file_path],
        )
        return FormattedFileContents(
            ok=proc.returncode == 0,
            formatted_file_contents=proc.stdout,
            error_message=proc.stderr.decode()
            if proc.returncode != 0
            else None,
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place using ``clang-format``.

        Returns:
            A FormatFixStatus that contains relevant errors/warnings.
        """
        self.format_files([file_path])
        # `clang-format` doesn't emit errors, and will always try its best to
        # format malformed files.
        return FormatFixStatus(ok=True, error_message=None)

    def format_files(
        self, paths: Iterable[Path], keep_warnings: bool = True
    ) -> Iterator[tuple[Path, FormatFixStatus]]:
        """Uses ``clang-format`` to format the specified files in-place.

        Returns:
            An iterator of ``Path`` and
            :py:class:`pw_presubmit.format.core.FormatFixStatus` pairs for each
            file that was not successfully formatted. If ``keep_warnings`` is
            ``True``, any successful format operations with warnings will also
            be returned.
        """
        self.run_tool(
            'clang-format',
            ['-i'] + self.clang_format_flags + list(paths),
            check=True,
        )

        # `clang-format` doesn't emit errors, and will always try its best to
        # format malformed files. For that reason, no errors are yielded.
        yield from []
