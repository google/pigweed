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
"""Formatter for Markdown files."""

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.whitespace import TrailingSpaceFormatter

DEFAULT_MARKDOWN_FILE_PATTERNS = FileFilter(endswith=['.md'])


# TODO: https://pwbug.dev/434034791 - Add real code formatting support for
# Markdown.


class MarkdownFormatter(TrailingSpaceFormatter):
    """A simple Markdown formatter that removes trailing whitespace."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'Markdown')
        kwargs.setdefault('file_patterns', DEFAULT_MARKDOWN_FILE_PATTERNS)
        super().__init__(**kwargs)
