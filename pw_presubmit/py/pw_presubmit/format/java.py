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
"""Formatter for Java files."""

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.cpp import ClangFormatFormatter

DEFAULT_JAVA_FILE_PATTERNS = FileFilter(endswith=['.java'])


class JavaFormatter(ClangFormatFormatter):
    """A Java formatter that uses clang-format."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'Java')
        kwargs.setdefault('file_patterns', DEFAULT_JAVA_FILE_PATTERNS)
        super().__init__(**kwargs)
