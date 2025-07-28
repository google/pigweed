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
"""Formatter for JSON files."""

import json
from pathlib import Path

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)

DEFAULT_JSON_FILE_PATTERNS = FileFilter(endswith=['.json'])


def _format_json_contents(contents: bytes) -> bytes:
    """Formats JSON file contents or raises json.JSONDecodeError."""
    return json.dumps(json.loads(contents), indent=2).encode() + b'\n'


class JsonFormatter(FileFormatter):
    """A simple JSON formatter."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'JSON')
        kwargs.setdefault('file_patterns', DEFAULT_JSON_FILE_PATTERNS)
        super().__init__(**kwargs)

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Formats JSON file contents."""
        try:
            formatted = _format_json_contents(file_contents)
        except json.JSONDecodeError as err:
            return FormattedFileContents(
                ok=False,
                formatted_file_contents=b'',
                error_message=(
                    f'{file_path}:{err.lineno}:{err.colno}: {err.msg}'
                ),
            )

        return FormattedFileContents(
            ok=True,
            formatted_file_contents=formatted,
            error_message=None,
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats a JSON file in place."""
        contents = file_path.read_bytes()
        try:
            formatted = _format_json_contents(contents)
            if formatted != contents:
                file_path.write_bytes(formatted)
        except json.JSONDecodeError as err:
            return FormatFixStatus(
                ok=False,
                error_message=(
                    f'{file_path}:{err.lineno}:{err.colno}: {err.msg}'
                ),
            )

        return FormatFixStatus(ok=True, error_message=None)
