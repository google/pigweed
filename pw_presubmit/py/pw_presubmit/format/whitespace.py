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
"""Code formatter plugin for whitespace."""

from pathlib import Path
import re

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)

_TRAILING_SPACE = re.compile(rb'[ \t]+$', flags=re.MULTILINE)


class TrailingSpaceFormatter(FileFormatter):
    """A formatter that removes trailing whitespace."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'Trailing Whitespace')
        kwargs.setdefault('file_patterns', FileFilter())
        super().__init__(**kwargs)

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Removes trailing whitespace from file_contents."""
        corrected = _TRAILING_SPACE.sub(b'', file_contents)
        return FormattedFileContents(
            ok=True,
            formatted_file_contents=corrected,
            error_message=None,
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Removes trailing whitespace from a file."""
        contents = file_path.read_bytes()
        corrected = _TRAILING_SPACE.sub(b'', contents)
        if corrected != contents:
            file_path.write_bytes(corrected)
        return FormatFixStatus(ok=True, error_message=None)
