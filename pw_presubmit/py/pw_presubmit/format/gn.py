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
"""Code formatter plugin for GN build files."""

from pathlib import Path
from typing import Final, Iterable, Iterator, Sequence, Tuple

from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)


class GnFormatter(FileFormatter):
    """A formatter that runs `gn format` on files."""

    DEFAULT_FLAGS: Final[Sequence[str]] = ()

    def __init__(self, tool_flags: Sequence[str] = DEFAULT_FLAGS, **kwargs):
        super().__init__(**kwargs)
        self.gn_format_flags = list(tool_flags)

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Uses ``gn format`` to check the formatting of the requested file.

        The file at ``file_path`` is NOT modified by this check.

        Returns:
            A populated
            :py:class:`pw_presubmit.format.core.FormattedFileContents` that
            contains either the result of formatting the file, or an error
            message.
        """
        # `gn format --dry-run` only signals which files need to be updated,
        # not what the effects are. To get the formatted result, we pipe the
        # file in via stdin and then return the result.
        proc = self.run_tool(
            'gn',
            ['format'] + self.gn_format_flags + ['--stdin'],
            input=file_contents,
        )
        ok = proc.returncode == 0
        return FormattedFileContents(
            ok=ok,
            formatted_file_contents=proc.stdout if ok else b'',
            # `gn format` emits errors over stdout, not stderr.
            error_message=None if ok else proc.stdout.decode(),
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place using ``gn format``.

        Returns:
            A FormatFixStatus that contains relevant errors/warnings.
        """
        proc = self.run_tool(
            'gn',
            ['format'] + self.gn_format_flags + [file_path],
        )
        ok = proc.returncode == 0
        return FormatFixStatus(
            ok=ok,
            # `gn format` emits errors over stdout, not stderr.
            error_message=None if ok else proc.stdout.decode(),
        )

    def format_files(
        self, paths: Iterable[Path], keep_warnings: bool = True
    ) -> Iterator[Tuple[Path, FormatFixStatus]]:
        """Uses ``gn format`` to fix formatting of the specified files in-place.

        Returns:
            An iterator of ``Path`` and
            :py:class:`pw_presubmit.format.core.FormatFixStatus` pairs for each
            file that was not successfully formatted. If ``keep_warnings`` is
            ``True``, any successful format operations with warnings will also
            be returned.
        """
        # Try to format all files in one `gn` command.
        proc = self.run_tool(
            'gn',
            ['format'] + self.gn_format_flags + list(paths),
        )

        # If there's an error, fall back to per-file formatting to figure out
        # which file has problems.
        if proc.returncode != 0:
            yield from super().format_files(paths, keep_warnings)
