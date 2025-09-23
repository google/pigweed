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
"""Formatter for CSS files."""

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.prettier import PrettierFormatter

DEFAULT_CSS_FILE_PATTERNS = FileFilter(endswith=['.css'])

# TODO: b/308948504 - Add real code formatting support for CSS


class CssFormatter(PrettierFormatter):
    """A CSS formatter that uses prettier."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'CSS')
        kwargs.setdefault('file_patterns', DEFAULT_CSS_FILE_PATTERNS)
        super().__init__(**kwargs)
