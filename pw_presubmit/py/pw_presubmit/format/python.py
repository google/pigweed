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
"""Code formatter plugins for Python."""

import os
from pathlib import Path
from typing import Iterable, Iterator, Optional, Sequence, Tuple, Union

from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)


class BlackFormatter(FileFormatter):
    """A formatter that runs ``black`` on files."""

    def __init__(self, config_file: Optional[Path], **kwargs):
        super().__init__(**kwargs)
        self.config_file = config_file

    def _config_file_args(self) -> Sequence[Union[str, Path]]:
        if self.config_file is not None:
            return ('--config', Path(self.config_file))

        return ()

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Uses ``black`` to check the formatting of the requested file.

        The file at ``file_path`` is NOT modified by this check.

        Returns:
            A populated
            :py:class:`pw_presubmit.format.core.FormattedFileContents` that
            contains either the result of formatting the file, or an error
            message.
        """
        proc = self.run_tool(
            'black',
            [*self._config_file_args(), '-q', '-'],
            input=file_contents,
        )
        ok = proc.returncode == 0
        formatted_file_contents = proc.stdout if ok else b''

        # On Windows, Black's stdout always has CRLF line endings.
        if os.name == 'nt':
            formatted_file_contents = formatted_file_contents.replace(
                b'\r\n', b'\n'
            )

        return FormattedFileContents(
            ok=ok,
            formatted_file_contents=formatted_file_contents,
            error_message=None if ok else proc.stderr.decode(),
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place using ``black``.

        Returns:
            A FormatFixStatus that contains relevant errors/warnings.
        """
        proc = self.run_tool(
            'black',
            [*self._config_file_args(), '-q', file_path],
        )
        ok = proc.returncode == 0
        return FormatFixStatus(
            ok=ok,
            error_message=None if ok else proc.stderr.decode(),
        )

    def format_files(
        self, paths: Iterable[Path], keep_warnings: bool = True
    ) -> Iterator[Tuple[Path, FormatFixStatus]]:
        """Uses ``black`` to format the specified files in-place.

        Returns:
            An iterator of ``Path`` and
            :py:class:`pw_presubmit.format.core.FormatFixStatus` pairs for each
            file that was not successfully formatted. If ``keep_warnings`` is
            ``True``, any successful format operations with warnings will also
            be returned.
        """
        proc = self.run_tool(
            'black',
            [*self._config_file_args(), '-q', *paths],
        )

        # If there's an error, fall back to per-file formatting to figure out
        # which file has problems.
        if proc.returncode != 0:
            yield from super().format_files(paths, keep_warnings)
